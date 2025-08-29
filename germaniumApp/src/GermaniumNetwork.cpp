/*
 * GermaniumNetwork.cpp
 * UDP network communication functions for Germanium areaDetector driver
 * Handles remote control and data acquisition from Zynq-based detector
 */

#include "Germanium.hpp"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <chrono>
#include <thread>

#include "libComAPI.h"
#include "osdSock.h"
#include "ellLib.h"

//===========================================================================//

int Germanium::make_udp_bind(int port)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0); 
    if(s == INVALID_SOCKET)
        return s;
    
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);

    if(bind(s, (sockaddr*)&a, sizeof(a)) < 0)
    {   
        printf("bind failed on %d\n", port);
        close(s);
        return INVALID_SOCKET;
    }   
    
    return s;
}

//===========================================================================//

void Germanium::set_nonblock( int s ) 
{
    int opt = 1;

    setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt) );
}

//===========================================================================//




/*
 * Initialize UDP sockets for communication with Zynq device
 * Returns true on success, false on failure
 */
bool Germanium::initializeUDPSockets()
{
    printf("Initializing UDP sockets for %s...\n", ipAddress);

    udpControlSocket = make_udp_bind( controlPort );
    udpDataSocket = make_udp_bind( dataPort );
    if ( udpControlSocket == INVALID_SOCKET || udpDataSocket == INVALID_SOCKET )
    {
        printf( "Failed to open UDP sockets\n" );
        return false;
    }

    set_nonblock( udpControlSocket );
    set_nonblock( udpDataSocket );

//    int opt = 1;
//
//    // Setup control address
//    memset(&controlAddr, 0, sizeof(controlAddr));
//    controlAddr.sin_family = AF_INET;
//    controlAddr.sin_port = htons(controlPort);
//    inet_pton(AF_INET, ipAddress, &controlAddr.sin_addr);
//
//    udpControlSocket = socket(AF_INET, SOCK_DGRAM, 0);
//    if (udpControlSocket < 0)
//    {
//        printf("Failed to create UDP control socket\n");
//        return false;
//    }
//
//    // Set socket options for reuse and non-blocking
//    setsockopt(udpControlSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
//
//    // Setup data address
//    memset(&dataAddr, 0, sizeof(dataAddr));
//    dataAddr.sin_family = AF_INET;
//    dataAddr.sin_port = htons(dataPort);
//    inet_pton(AF_INET, ipAddress, &dataAddr.sin_addr);
//
//    udpDataSocket = socket(AF_INET, SOCK_DGRAM, 0); 
//    if (udpDataSocket < 0)
//    {   
//        printf("Failed to create UDP data socket\n");
//        close(udpControlSocket);
//        return false;
//    }   
//
//    setsockopt(udpDataSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
//    
//    // Bind data socket to receive data packets
//    struct sockaddr_in dataBindAddr;
//    memset(&dataBindAddr, 0, sizeof(dataBindAddr));
//    dataBindAddr.sin_family = AF_INET;
//    dataBindAddr.sin_addr.s_addr = INADDR_ANY;
//    dataBindAddr.sin_port = htons(dataPort);
//    
//    if (bind(udpDataSocket, (struct sockaddr*)&dataBindAddr, sizeof(dataBindAddr)) < 0) {
//        printf("Germanium: Failed to bind data socket to port %d: %s\n", 
//               dataPort, strerror(errno));
//        close(udpControlSocket);
//        close(udpDataSocket);
//        return false;
//    }
//    
    // UDP data buffer now allocated in constructor using smart pointer
    // Just reset the buffer size
    dataBufferSize = 0;
    
    // Create mutex for UDP socket access
    udpMutex = epicsMutexCreate();
    
    // Create event for data processing synchronization
    dataAvailable = epicsEventCreate(epicsEventEmpty);
    
    udpInitialized = true;
    printf("Germanium: UDP sockets initialized - Control: %s:%d, Data port: %d\n",
           ipAddress, controlPort, dataPort);
    
    return true;
}

//===========================================================================//

/*
 * Close UDP sockets and cleanup network resources
 */
void Germanium::closeUDPSockets()
{
    udpInitialized = false;
    threadsRunning = false;
    
    // Close sockets
    if (udpControlSocket >= 0) {
        close(udpControlSocket);
        udpControlSocket = -1;
    }
    
    if (udpDataSocket >= 0) {
        close(udpDataSocket);
        udpDataSocket = -1;
    }
    
    // Cleanup resources - smart pointer handles udpDataBuffer automatically
    // No manual cleanup needed for udpDataBuffer
    
    if (udpMutex) {
        epicsMutexDestroy(udpMutex);
        udpMutex = nullptr;
    }
    
    if (dataAvailable) {
        epicsEventDestroy(dataAvailable);
        dataAvailable = nullptr;
    }
    
    printf("Germanium: UDP sockets closed\n");
}

//===========================================================================//

/*
 * Send UDP command to Zynq device using proper message format
 * Returns asynSuccess on success, asynError on failure
 */
asynStatus Germanium::sendUDPCommand( uint16_t op, uint32_t data )
{
    if (!udpInitialized) {
        return asynError;
    }
    
    // Create proper UDP message structure
    UdpReqMsg msg;
    msg.id = 0x1234;  // Fixed ID for now - could be incremental
    size_t msgSize;
    
    // Determine if this is a read or write operation based on command
    if ( op & 0x8000 )
    {
        msgSize = 4;
    } else {
        msgSize = 8;
        msg.payload.single_word.data = htonl(data);
    }
    
    // Set destination port for control commands
    deviceAddr.sin_port = htons(controlPort);
    
    // Send command with mutex protection
    epicsMutexLock(udpMutex);
    ssize_t sent = sendto( udpControlSocket
                         , &msg
                         , msgSize
                         , 0
                         , (struct sockaddr*)&deviceAddr
                         , sizeof(deviceAddr)
                         );
    epicsMutexUnlock(udpMutex);
    
    if (sent != msgSize)
    {
        printf( "Germanium: Failed to send UDP command 0x%x: %s\n"
              , op
              , strerror(errno)
              );
        return asynError;
    }
    
    return asynSuccess;
}

//===========================================================================//


/*
 * UDP-based register write (replaces direct pl_register_write)
 */
asynStatus Germanium::udpRegisterWrite(uint32_t reg, uint32_t value)
{
    return sendUDPCommand( reg, value );
}

/*
 * UDP-based register read (replaces direct pl_register_read)
 */
asynStatus Germanium::udpRegisterRead(uint32_t reg)
{
    return sendUDPCommand( 0x8000 | reg, 0 );
}

/*
 * ADC configuration via UDP command using proper message format
 */
void Germanium::ad9252_cnfg(int adc, int reg, int value)
{
    if (!udpInitialized) {
        printf("Germanium: UDP not initialized for ADC config\n");
        return;
    }
    
    // Create proper UDP message for ADC clock skew configuration
    UdpReqMsg msg;
    msg.id = 0x1236;  // Different ID for ADC config
    msg.op = makeWriteOp(0x4000);  // Special operation code for ADC config
    
    // Use the AdcClkSkewReqMsgPayload structure
    msg.payload.ad9252_clk_skew.chip_num = htons(adc);
    msg.payload.ad9252_clk_skew.skew = htons(value);
    
    // Set destination port for control commands
    deviceAddr.sin_port = htons(controlPort);
    
    // Send the message
    epicsMutexLock(udpMutex);
    ssize_t sent = sendto(udpControlSocket, &msg, sizeof(msg), 0,
                         (struct sockaddr*)&deviceAddr, sizeof(deviceAddr));
    epicsMutexUnlock(udpMutex);
    
    if (sent != sizeof(msg)) {
        printf("Germanium: Failed to send ADC config: %s\n", strerror(errno));
    } else {
        printf("Germanium: ADC[%d] configured with skew=%d\n", adc, value);
    }
}

//===========================================================================//

/*
 * Send MARS ASIC configuration array via UDP using proper message format
 * This sends the loads array using the StuffMarsReqMsgPayload structure
 */
asynStatus Germanium::sendMarsConfiguration()
{
    if (!udpInitialized) {
        printf("Germanium: UDP not initialized, cannot send MARS configuration\n");
        return asynError;
    }
    
    // Create proper UDP message for MARS configuration
    UdpReqMsg msg;
    msg.id = 0x1235;  // Different ID for MARS config
    msg.op = makeWriteOp(0x3000);  // Special operation code for MARS config
    
    memcpy(msg.payload.stuff_mars.loads, loads, sizeof(loads));
    
    // Set destination
    struct sockaddr_in deviceAddr;
    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_port = htons(controlPort);
    inet_pton(AF_INET, ipAddress, &deviceAddr.sin_addr);

    // Send the configuration data with mutex protection
    epicsMutexLock(udpMutex);
    ssize_t sent = sendto( udpControlSocket
                         , &msg
                         , sizeof(msg)
                         , 0
                         , (struct sockaddr*)&deviceAddr
                         , sizeof(deviceAddr)
                         );
    epicsMutexUnlock(udpMutex);
    
    if (sent != sizeof(msg))
    {
        printf( "Failed to send MARS configuration: %s\n"
              , strerror(errno)
              );

        return asynError;
    }
    
    printf( "Sent MARS configuration (%zu bytes) for %d chips\n"
          , sizeof(msg.payload.stuff_mars)
          , nchips
          );

    return asynSuccess;
}

//===========================================================================//

/*
 * Write integer array via UDP (generic function for array transfers)
 * This can be used for channel enable arrays, threshold arrays, etc.
 */
//asynStatus Germanium::udpWriteIntArray(uint32_t command, const void *data, 
//                                       size_t dataSize, uint32_t address)
//{
//    if (!udpInitialized) {
//        return asynError;
//    }
//    
//    if (dataSize > UDP_BUFFER_SIZE - sizeof(UDPCommand)) {
//        printf("Germanium: Array data too large (%zu bytes) for UDP transfer\n", dataSize);
//        return asynError;
//    }
//    
//    // Create buffer for command + data
//    uint8_t *buffer = new uint8_t[sizeof(UDPCommand) + dataSize];
//    UDPCommand *header = (UDPCommand*)buffer;
//    uint8_t *payload = buffer + sizeof(UDPCommand);
//    
//    // Fill header
//    header->command = htonl(command);
//    header->address = htonl(address);
//    header->data = htonl(dataSize);
//    header->checksum = htonl(command ^ address ^ dataSize);
//    
//    // Copy data
//    memcpy(payload, data, dataSize);
//    
//    // Send with mutex protection
//    deviceAddr.sin_port = htons(controlPort);
//    
//    epicsMutexLock(udpMutex);
//    ssize_t sent = sendto(udpControlSocket, buffer, sizeof(UDPCommand) + dataSize, 0,
//                         (struct sockaddr*)&deviceAddr, sizeof(deviceAddr));
//    epicsMutexUnlock(udpMutex);
//    
//    delete[] buffer;
//    
//    if (sent != (ssize_t)(sizeof(UDPCommand) + dataSize)) {
//        printf("Germanium: Failed to send array data (cmd=0x%x): %s\n", 
//               command, strerror(errno));
//        return asynError;
//    }
//    
//    return asynSuccess;
//}

/*
 * Send a string command via UDP
 */
//asynStatus Germanium::udpSendString(uint32_t command, const char *str) {
//    if (!udpInitialized || udpControlSocket < 0) {
//        printf("Germanium: UDP not initialized for string command\n");
//        return asynError;
//    }
//    
//    // Create UDP command packet with string payload
//    size_t strLen = strlen(str);
//    size_t packetSize = sizeof(UDPCommand) + strLen + 1; // +1 for null terminator
//    
//    uint8_t *packet = new uint8_t[packetSize];
//    UDPCommand *cmd = (UDPCommand*)packet;
//    
//    cmd->command = command;
//    cmd->address = 0;
//    cmd->data_length = strLen + 1;
//    
//    // Copy string after the command header
//    strcpy((char*)(packet + sizeof(UDPCommand)), str);
//    
//    // Send packet
//    struct sockaddr_in addr;
//    addr.sin_family = AF_INET;
//    addr.sin_port = htons(controlPort);
//    inet_pton(AF_INET, ipAddress, &addr.sin_addr);
//    
//    ssize_t sent = sendto(udpControlSocket, packet, packetSize, 0,
//                         (struct sockaddr*)&addr, sizeof(addr));
//    
//    delete[] packet;
//    
//    if (sent < 0) {
//        printf("Germanium: Failed to send string command 0x%02X: %s\n", 
//               command, strerror(errno));
//        return asynError;
//    }
//    
//    printf("Germanium: Sent string command 0x%02X: '%s'\n", command, str);
//    return asynSuccess;
//}


//===========================================================================//

/*
 * UDP data reception thread function (C wrapper)
 */
extern "C" void udpDataThreadC(void *drvPvt)
{
    Germanium *pGermanium = static_cast<Germanium*>(drvPvt);
    pGermanium->udpDataThread();
}

//===========================================================================//

/*
 * UDP data reception thread - handles incoming data packets
 */
void Germanium::udpDataThread()
{
    printf("UDP data reception thread started\n");
    
    uint8_t receiveBuffer[UDP_BUFFER_SIZE];
    struct sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);
    
    while (threadsRunning)
    {
        // Wait for data with timeout
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(udpDataSocket, &readfds);
        timeout.tv_sec = 1;   // 1 second timeout
        timeout.tv_usec = 0;
    
        int result = select(udpDataSocket + 1, &readfds, nullptr, nullptr, &timeout);
    
        if (result > 0 && FD_ISSET(udpDataSocket, &readfds))
        {   
            // Receive data packet
            ssize_t bytesReceived = recvfrom(udpDataSocket, receiveBuffer, sizeof(receiveBuffer), 0,
                                           (struct sockaddr*)&senderAddr, &senderAddrLen);

            if (bytesReceived > 0)
            {
                // Process received data
                processReceivedData(receiveBuffer, static_cast<size_t>(bytesReceived));
            }
            else if (bytesReceived < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                printf("Error receiving UDP data: %s\n", strerror(errno));
            }
        }
        else if (result < 0 && errno != EINTR)
        {
            printf("Error in UDP data select: %s\n", strerror(errno));
        }

        // Brief pause to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    printf("UDP data reception thread stopped\n");
}

//===========================================================================//

/*
 * UDP control reception thread function (C wrapper)
 */
extern "C" void udpControlThreadC(void *drvPvt)
{
    Germanium *pGermanium = static_cast<Germanium*>(drvPvt);
    pGermanium->udpControlThread();
}

//===========================================================================//

