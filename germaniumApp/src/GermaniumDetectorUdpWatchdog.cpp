/**
 * @file GermaniumDetectorUdpWatchdog.cpp
 * @brief PL UDP watchdog and UDP register reachability checks.
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

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

//===========================================================================//

namespace {

constexpr uint32_t GIGE_KEY = 0xdeadbeef;

constexpr uint16_t GIGE_REGISTER_WRITE_TX_PORT = 0x7D00;
constexpr uint16_t GIGE_REGISTER_READ_TX_PORT  = 0x7D01;
constexpr uint16_t GIGE_REGISTER_RX_PORT       = 0x7D02;

// Address in the detector UDP register protocol, not the ZMQ register map.
constexpr uint32_t UDP_CONTROL_REGISTER = 0x00000001;
constexpr uint32_t UDP_ENABLE_VALUE = 0x1;

constexpr double UDP_WATCHDOG_TICK_SEC = 1.0;
constexpr double UDP_WATCHDOG_PERIOD_SEC = 30.0;
constexpr double UDP_REINIT_RETRY_PERIOD_SEC = 10.0;

} // namespace

//===========================================================================//

void GermaniumDetector::setAcquisitionRunning(bool running)
{
    const bool previous = acquisitionRunning.exchange(running);
    if (poller)
        poller->setFast(running);

    if (previous != running )
        udpWatchdogEvent.wait(UDP_WATCHDOG_TICK_SEC);
}

//===========================================================================//

void GermaniumDetector::requestUdpReinitialization()
{
    udpInitRequested.store(true);
    udpWatchdogEvent.wait(UDP_WATCHDOG_TICK_SEC);
}

//===========================================================================//

bool GermaniumDetector::initializeUdpRegisterSocket()
{
    if (udpRegisterSocket >= 0)
        return true;

    udpRegisterSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (udpRegisterSocket < 0)
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: failed to create UDP register socket: %s\n"
                 , __func__
                 , strerror(errno)
                 );
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
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: failed to bind UDP register socket to port %x, error: %s\n"
                 , __func__
                 , GIGE_REGISTER_RX_PORT
                 , strerror(errno)
                 );
        closeUdpRegisterSocket();
        return false;
    }
    std::cerr << __func__
              << ": bound UDP register socket to port "
              <<  GIGE_REGISTER_RX_PORT
              << "\n";

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
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: no valid configured PL UDP IP address ('%s')\n"
                 , __func__
                 , buf
                 );
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
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "[%s]: UDP watchdog thread started\n"
             , __func__
             );

    double idleElapsed = 0.0;
    double reinitRetryElapsed = UDP_REINIT_RETRY_PERIOD_SEC;

    while (threadsRunning.load())
    {
        udpWatchdogEvent.wait(UDP_WATCHDOG_TICK_SEC);

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

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "[%s]: UDP watchdog thread stopped\n"
             , __func__
             );
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

    bool writeOk = udpRegisterWrite(targetAddress, UDP_CONTROL_REGISTER, UDP_ENABLE_VALUE);
    //std::print("{}: PL UDP register write {}\n", __func__, (writeOk ? "successful" : "failed"));

    uint32_t value = 0;
    bool readOk = udpRegisterRead(targetAddress, UDP_CONTROL_REGISTER, value);
    //std::print("{}: PL UDP register read {}\n", __func__, ( readOk ? "successful" : "failed" ));

    bool reachable = writeOk && readOk && (value == UDP_ENABLE_VALUE);

    setUdpReachable(reachable);

    if (!reachable)
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: PL UDP initialization failed (write=%d read=%d value=0x%08X)\n"
                 , __func__
                 , writeOk ? 1 : 0
                 , readOk ? 1 : 0
                 , value
                 );
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

    uint32_t value = 0;
    bool readOk = udpRegisterRead(targetAddress, UDP_CONTROL_REGISTER, value);
    setUdpReachable(readOk);
}

//===========================================================================//

bool GermaniumDetector::udpRegisterWrite(const std::string& targetAddress,
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
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: UDP register write send failed: %s\n"
                 , __func__
                 , strerror(errno)
                 );
        return false;
    }
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "[%s]: UDP register write send successful. GIGE_KEY = %x, addr = %x, value = %x\n"
             , __func__
             , GIGE_KEY
             , addr
             , value
             );    

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(udpRegisterSocket, &fds);

    timeval timeout {};
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    int ret = select(udpRegisterSocket + 1, &fds, nullptr, nullptr, &timeout);
    if (ret <= 0)
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: select() failed: %s\n"
                 , __func__
                 , ( ret== 0 ) ? "timeout" : strerror(errno)
                 );
        return false;
    }

    uint32_t reply[3] {};
    sockaddr_in src {};
    socklen_t srcLen = sizeof(src);
    ssize_t received = recvfrom(udpRegisterSocket, reply, sizeof(reply), 0,
                                reinterpret_cast<sockaddr*>(&src), &srcLen);
    if (received < static_cast<ssize_t>(3 * sizeof(uint32_t)))
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: recvfrom() failed\n"
                 , __func__
                 );
        return false;
    }

    uint32_t replyAddr = ntohl(reply[0]);
    uint32_t returnedValue = ntohl(reply[1]);
    uint32_t status = reply[2];
    //std::print("{}: received addr = {}, value = {}, status = {}\n", __func__, replyAddr, returnedValue, status);

    return (replyAddr == addr) && (returnedValue == value) && (status == 1);
}

//===========================================================================//

bool GermaniumDetector::udpRegisterRead(const std::string& targetAddress,
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
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: UDP register read send failed: %s\n"
                 , __func__
                 , strerror(errno)
                 );
        return false;
    }
    
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "[%s]: UDP register read succeeded. msg[0] = %x, msg[1] = %x\n"
             , __func__
             , msg[0]
             , msg[1]
             );

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(udpRegisterSocket, &fds);

    timeval timeout {};
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    int ret = select(udpRegisterSocket + 1, &fds, nullptr, nullptr, &timeout);
    if (ret <= 0)
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: select() failed: %s\n"
                 , __func__
                  , (ret==0) ? "timeout" : strerror(errno)
                 );
        return false;
    }
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "[%s]: select() succeeded\n"
             , __func__
             );

    uint32_t reply[3] {};
    sockaddr_in src {};
    socklen_t srcLen = sizeof(src);
    ssize_t received = recvfrom(udpRegisterSocket, reply, sizeof(reply), 0,
                                reinterpret_cast<sockaddr*>(&src), &srcLen);
    if (received < static_cast<ssize_t>(3 * sizeof(uint32_t)))
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: recvfrom() failed\n"
                 , __func__
                 );
        return false;
    }

    uint32_t replyAddr = ntohl(reply[0]);
    if (replyAddr != addr)
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "[%s]: unexpected UDP register read address: %x\n"
                 , __func__
                 , replyAddr
                 );
        return false;
    }

    value = reply[2];
    return true;
}

//===========================================================================//
