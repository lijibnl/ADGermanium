/**
 * @file GermaniumDetectorUdpWatchdog.cpp
 * @brief PL UDP ARP watchdog and legacy UDP register reachability checks.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * 
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */
//===========================================================================//

#include "GermaniumDetector.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <ifaddrs.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

//===========================================================================//

namespace {

constexpr uint32_t GIGE_KEY = 0xdeadbeef;
constexpr uint32_t GIGE_REGISTER_OKAY = 0x4f6b6179;
constexpr uint32_t GIGE_REGISTER_FAIL = 0x4661696c;

constexpr uint16_t GIGE_REGISTER_WRITE_TX_PORT = 0x7D00;
constexpr uint16_t GIGE_REGISTER_READ_TX_PORT  = 0x7D01;
constexpr uint16_t GIGE_REGISTER_RX_PORT       = 0x7D02;

constexpr uint32_t UDP_ENABLE_REGISTER = GermaniumProtocol::Register::LEDS;
constexpr uint32_t UDP_ENABLE_VALUE = 0x1;

constexpr double UDP_WATCHDOG_TICK_SEC = 1.0;
constexpr double UDP_WATCHDOG_PERIOD_SEC = 30.0;
constexpr double UDP_REINIT_RETRY_PERIOD_SEC = 10.0;

struct ArpInterface
{
    char name[IFNAMSIZ] {};
    int ifIndex {-1};
    in_addr ip {};
    in_addr netmask {};
    std::array<uint8_t, ETH_ALEN> mac {};
};

struct ArpPacket
{
    uint8_t  ethDst[ETH_ALEN];
    uint8_t  ethSrc[ETH_ALEN];
    uint16_t ethType;
    uint16_t hwType;
    uint16_t protoType;
    uint8_t  hwSize;
    uint8_t  protoSize;
    uint16_t opCode;
    uint8_t  senderMac[ETH_ALEN];
    uint8_t  senderIp[4];
    uint8_t  targetMac[ETH_ALEN];
    uint8_t  targetIp[4];
} __attribute__((packed));

bool sameSubnet(in_addr lhs, in_addr rhs, in_addr mask)
{
    return (lhs.s_addr & mask.s_addr) == (rhs.s_addr & mask.s_addr);
}

bool findInterfaceForTarget(in_addr target, ArpInterface& out)
{
    ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0)
        return false;

    bool found = false;
    for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || !ifa->ifa_netmask)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (!(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK))
            continue;

        auto *addr = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
        auto *mask = reinterpret_cast<sockaddr_in*>(ifa->ifa_netmask);
        if (!sameSubnet(addr->sin_addr, target, mask->sin_addr))
            continue;

        std::strncpy(out.name, ifa->ifa_name, sizeof(out.name) - 1);
        out.name[sizeof(out.name) - 1] = '\0';
        out.ip = addr->sin_addr;
        out.netmask = mask->sin_addr;
        found = true;
        break;
    }

    freeifaddrs(ifaddr);
    if (!found)
        return false;

    out.ifIndex = if_nametoindex(out.name);
    if (out.ifIndex <= 0)
        return false;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return false;

    ifreq ifr {};
    std::memcpy(ifr.ifr_name, out.name, sizeof(ifr.ifr_name));
    bool macOk = ioctl(fd, SIOCGIFHWADDR, &ifr) == 0;
    close(fd);
    if (!macOk)
        return false;

    std::memcpy(out.mac.data(), ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    return true;
}

} // namespace

//===========================================================================//

void GermaniumDetector::setAcquisitionRunning(bool running)
{
    const bool previous = acquisitionRunning.exchange(running);
    if (poller)
        poller->setFast(running);

    if (previous != running && udpWatchdogEvent)
        epicsEventSignal(udpWatchdogEvent);
}

//===========================================================================//

void GermaniumDetector::requestUdpReinitialization()
{
    udpInitRequested.store(true);
    if (udpWatchdogEvent)
        epicsEventSignal(udpWatchdogEvent);
}

//===========================================================================//

bool GermaniumDetector::initializeUdpRegisterSocket()
{
    if (udpRegisterSocket >= 0)
        return true;

    udpRegisterSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpRegisterSocket < 0)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: failed to create UDP register socket: %s\n",
                  portName, strerror(errno));
        return false;
    }

    int opt = 1;
    setsockopt(udpRegisterSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in bindAddr {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port = htons(GIGE_REGISTER_RX_PORT);

    if (bind(udpRegisterSocket, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: failed to bind UDP register socket to port %u: %s\n",
                  portName, GIGE_REGISTER_RX_PORT, strerror(errno));
        closeUdpRegisterSocket();
        return false;
    }

    udpRegisterInitialized = true;
    return true;
}

//===========================================================================//

void GermaniumDetector::closeUdpRegisterSocket()
{
    udpRegisterInitialized = false;
    if (udpRegisterSocket >= 0)
    {
        close(udpRegisterSocket);
        udpRegisterSocket = -1;
    }
}

//===========================================================================//

bool GermaniumDetector::getConfiguredUdpAddress(std::string& address)
{
    char buf[64] {};
    getStringParam(GermaniumIPADDR, sizeof(buf), buf);
    if (buf[0] == '\0')
        getStringParam(GermaniumIPADDR_RBV, sizeof(buf), buf);

    in_addr addr {};
    if (inet_pton(AF_INET, buf, &addr) != 1)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: no valid configured PL UDP IP address ('%s')\n",
                  portName, buf);
        return false;
    }

    address = buf;
    return true;
}

//===========================================================================//

void GermaniumDetector::setUdpReachable(bool reachable)
{
    setIntegerParam(GermaniumUDPReachable_RBV, reachable ? 1 : 0);
    callParamCallbacks();
}

//===========================================================================//

void GermaniumDetector::udpWatchdogThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->udpWatchdogThread();
}

//===========================================================================//

void GermaniumDetector::udpWatchdogThread()
{
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s: UDP watchdog thread started\n", portName);

    double idleElapsed = 0.0;
    double reinitRetryElapsed = UDP_REINIT_RETRY_PERIOD_SEC;

    while (threadsRunning.load())
    {
        epicsEventWaitWithTimeout(udpWatchdogEvent, UDP_WATCHDOG_TICK_SEC);

        if (udpInitRequested.exchange(false))
        {
            runUdpInitialization();
            idleElapsed = 0.0;
            reinitRetryElapsed = 0.0;
            continue;
        }

        reinitRetryElapsed += UDP_WATCHDOG_TICK_SEC;
        if (!udpRegisterInitialized && reinitRetryElapsed >= UDP_REINIT_RETRY_PERIOD_SEC)
        {
            requestUdpReinitialization();
            continue;
        }

        if (acquisitionRunning.load())
        {
            idleElapsed = 0.0;
            continue;
        }

        idleElapsed += UDP_WATCHDOG_TICK_SEC;
        if (idleElapsed >= UDP_WATCHDOG_PERIOD_SEC)
        {
            runUdpWatchdogProbe();
            idleElapsed = 0.0;
        }
    }

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s: UDP watchdog thread stopped\n", portName);
}

//===========================================================================//

void GermaniumDetector::runUdpInitialization()
{
    closeUdpRegisterSocket();
    setUdpReachable(false);

    std::string targetAddress;
    if (!getConfiguredUdpAddress(targetAddress))
    {
        return;
    }

    in_addr configuredAddr {};
    if (inet_pton(AF_INET, targetAddress.c_str(), &configuredAddr) == 1)
    {
        zmqTx(GermaniumProtocol::Command::REG_WRITE, GermaniumProtocol::Register::UDP_IP_ADDR, ntohl(configuredAddr.s_addr));
        epicsThreadSleep(0.1);
    }

    bool arpOk = sendUdpArpRequest(targetAddress);
    bool writeOk = legacyUdpRegisterWrite(targetAddress, UDP_ENABLE_REGISTER, UDP_ENABLE_VALUE);

    uint32_t value = 0;
    bool readOk = legacyUdpRegisterRead(targetAddress, UDP_ENABLE_REGISTER, value);
    bool reachable = writeOk && readOk && (value == UDP_ENABLE_VALUE);

    setUdpReachable(reachable);

    if (!arpOk || !reachable)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: PL UDP initialization %s (arp=%d write=%d read=%d value=0x%08X)\n",
                  portName, reachable ? "partially succeeded" : "failed",
                  arpOk ? 1 : 0, writeOk ? 1 : 0, readOk ? 1 : 0, value);
    }
}

//===========================================================================//

void GermaniumDetector::runUdpWatchdogProbe()
{
    std::string targetAddress;
    if (!getConfiguredUdpAddress(targetAddress))
    {
        setUdpReachable(false);
        return;
    }

    sendUdpArpRequest(targetAddress);

    uint32_t value = 0;
    bool readOk = legacyUdpRegisterRead(targetAddress, UDP_ENABLE_REGISTER, value);
    setUdpReachable(readOk);
}

//===========================================================================//

bool GermaniumDetector::sendUdpArpRequest(const std::string& targetAddress)
{
    in_addr target {};
    if (inet_pton(AF_INET, targetAddress.c_str(), &target) != 1)
        return false;

    ArpInterface iface {};
    if (!findInterfaceForTarget(target, iface))
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: failed to find local interface for ARP target %s\n",
                  portName, targetAddress.c_str());
        return false;
    }

    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (fd < 0)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: failed to create raw ARP socket: %s\n",
                  portName, strerror(errno));
        return false;
    }

    ArpPacket pkt {};
    std::memset(pkt.ethDst, 0xff, ETH_ALEN);
    std::memcpy(pkt.ethSrc, iface.mac.data(), ETH_ALEN);
    pkt.ethType = htons(ETH_P_ARP);
    pkt.hwType = htons(1);
    pkt.protoType = htons(ETH_P_IP);
    pkt.hwSize = ETH_ALEN;
    pkt.protoSize = 4;
    pkt.opCode = htons(1);
    std::memcpy(pkt.senderMac, iface.mac.data(), ETH_ALEN);
    std::memcpy(pkt.senderIp, &iface.ip.s_addr, sizeof(pkt.senderIp));
    std::memset(pkt.targetMac, 0x00, ETH_ALEN);
    std::memcpy(pkt.targetIp, &target.s_addr, sizeof(pkt.targetIp));

    sockaddr_ll dest {};
    dest.sll_family = AF_PACKET;
    dest.sll_protocol = htons(ETH_P_ARP);
    dest.sll_ifindex = iface.ifIndex;
    dest.sll_halen = ETH_ALEN;
    std::memset(dest.sll_addr, 0xff, ETH_ALEN);

    ssize_t sent = sendto(fd, &pkt, sizeof(pkt), 0,
                          reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    int savedErrno = errno;
    close(fd);

    if (sent != static_cast<ssize_t>(sizeof(pkt)))
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: failed to send ARP request to %s on %s: %s\n",
                  portName, targetAddress.c_str(), iface.name, strerror(savedErrno));
        return false;
    }

    return true;
}

//===========================================================================//

bool GermaniumDetector::legacyUdpRegisterWrite(const std::string& targetAddress,
                                               uint32_t addr,
                                               uint32_t value)
{
    if (!initializeUdpRegisterSocket())
        return false;

    in_addr target {};
    if (inet_pton(AF_INET, targetAddress.c_str(), &target) != 1)
        return false;

    sockaddr_in dest {};
    dest.sin_family = AF_INET;
    dest.sin_addr = target;
    dest.sin_port = htons(GIGE_REGISTER_WRITE_TX_PORT);

    uint32_t msg[3] {};
    msg[0] = htonl(GIGE_KEY);
    msg[1] = htonl(addr);
    msg[2] = htonl(value);

    ssize_t sent = sendto(udpRegisterSocket, msg, sizeof(msg), 0,
                          reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    if (sent != static_cast<ssize_t>(sizeof(msg)))
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: UDP register write send failed: %s\n",
                  portName, strerror(errno));
        return false;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(udpRegisterSocket, &fds);

    timeval timeout {};
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    int ret = select(udpRegisterSocket + 1, &fds, nullptr, nullptr, &timeout);
    if (ret <= 0)
        return false;

    uint32_t reply[3] {};
    sockaddr_in src {};
    socklen_t srcLen = sizeof(src);
    ssize_t received = recvfrom(udpRegisterSocket, reply, sizeof(reply), 0,
                                reinterpret_cast<sockaddr*>(&src), &srcLen);
    if (received < static_cast<ssize_t>(2 * sizeof(uint32_t)))
        return false;

    uint32_t status = ntohl(reply[1]);
    return status == GIGE_REGISTER_OKAY;
}

//===========================================================================//

bool GermaniumDetector::legacyUdpRegisterRead(const std::string& targetAddress,
                                              uint32_t addr,
                                              uint32_t& value)
{
    value = 0;
    if (!initializeUdpRegisterSocket())
        return false;

    in_addr target {};
    if (inet_pton(AF_INET, targetAddress.c_str(), &target) != 1)
        return false;

    sockaddr_in dest {};
    dest.sin_family = AF_INET;
    dest.sin_addr = target;
    dest.sin_port = htons(GIGE_REGISTER_READ_TX_PORT);

    uint32_t msg[2] {};
    msg[0] = htonl(GIGE_KEY);
    msg[1] = htonl(addr);

    ssize_t sent = sendto(udpRegisterSocket, msg, sizeof(msg), 0,
                          reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    if (sent != static_cast<ssize_t>(sizeof(msg)))
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: UDP register read send failed: %s\n",
                  portName, strerror(errno));
        return false;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(udpRegisterSocket, &fds);

    timeval timeout {};
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    int ret = select(udpRegisterSocket + 1, &fds, nullptr, nullptr, &timeout);
    if (ret <= 0)
        return false;

    uint32_t reply[3] {};
    sockaddr_in src {};
    socklen_t srcLen = sizeof(src);
    ssize_t received = recvfrom(udpRegisterSocket, reply, sizeof(reply), 0,
                                reinterpret_cast<sockaddr*>(&src), &srcLen);
    if (received < static_cast<ssize_t>(2 * sizeof(uint32_t)))
        return false;

    if (ntohl(reply[1]) == GIGE_REGISTER_FAIL && ((ntohl(reply[0]) >> 24) == 0xff))
        return false;

    value = ntohl(reply[1]);
    return true;
}

//===========================================================================//
