/**
 * @file germaniumDetectorNetwork.cpp
 * @brief UDP network communication functions for germaniumDetector areaDetector
 *        driver.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "germaniumDetector.hpp"
#include "errlog.h"
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

int germaniumDetector::make_udp_bind(int port)
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

void germaniumDetector::set_nonblock( int s ) 
{
    int opt = 1;

    setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt) );
}

//===========================================================================//

/*
 * Initialize UDP sockets for communication with Zynq device
 * Returns true on success, false on failure
 */
bool germaniumDetector::initializeUDPSockets()
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
void germaniumDetector::closeUDPSockets()
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
asynStatus germaniumDetector::sendUDPCommand( uint16_t op, uint32_t data )
{
    if (!udpInitialized) {
        return asynError;
    }

    errlogPrintf( "[%s]: op = 0x%x, data = 0x%x\n", __func__, op, data );
    
    // Create proper UDP message structure
    UdpReqMsg msg;
    msg.id = 0x1234;  // Fixed ID for now - could be incremental
    msg.op = op;
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
    size_t sent = sendto( udpControlSocket
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
    else
    {
        errlogPrintf( "[%s]: msg.id = 0x%x, msg.op = 0x%x, msg.data = 0x%x, msg.size = %lu\n"
                    , __func__
                    , msg.id
                    , msg.op
                    //, msg.payload.single_word.data
                    , ntohl(msg.payload.single_word.data)
                    , sent
                    );

        for( int i=0; i<sent/2; i++ )
            errlogPrintf( "[%s]: %x\n", __func__, *((uint16_t*)(&msg)+i) );
    }
    
    return asynSuccess;
}

//===========================================================================//


/*
 * UDP-based register write (replaces direct pl_register_write)
 */
asynStatus germaniumDetector::udpRegisterWrite(uint32_t reg, uint32_t value)
{
    return sendUDPCommand( reg, value );
}

/*
 * UDP-based register read (replaces direct pl_register_read)
 */
asynStatus germaniumDetector::udpRegisterRead(uint32_t reg)
{
    return sendUDPCommand( 0x8000 | reg, 0 );
}

/*
 * ADC configuration via UDP command using proper message format
 */
asynStatus germaniumDetector::ad9252_cnfg(int adc, int value)
{
    if (!udpInitialized) {
        printf("Germanium: UDP not initialized for ADC config\n");
        return asynError;
    }
    
    // Create proper UDP message for ADC clock skew configuration
    UdpReqMsg msg;
    msg.id = 0x1236;  // Different ID for ADC config
    msg.op = makeWriteOp(ADC_SKEW);  // Special operation code for ADC config
    
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
    
    if (sent != sizeof(msg))
    {
        printf("Germanium: Failed to send ADC config: %s\n", strerror(errno));
        return asynError;
    }
    else
    {
        printf("Germanium: ADC[%d] configured with skew=%d\n", adc, value);
        return asynSuccess;
    }
}

//===========================================================================//

/*
 * Send MARS ASIC configuration array via UDP using proper message format
 * This sends the loads array using the StuffMarsReqMsgPayload structure
 */
asynStatus germaniumDetector::sendMarsConfiguration()
{
    if (!udpInitialized) {
        printf("Germanium: UDP not initialized, cannot send MARS configuration\n");
        return asynError;
    }

    wrap();
    
    // Create proper UDP message for MARS configuration
    UdpReqMsg msg;
    msg.id = 0x1235;  // Different ID for MARS config
    msg.op = makeWriteOp(STUFF_MARS);  // Special operation code for MARS config
    
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
          , nchips_
          );

    return asynSuccess;
}

//===========================================================================//

/*
 * UDP data reception thread function (C wrapper)
 */
//extern "C"
void germaniumDetector::udpDataThreadC(void *drvPvt)
{
    germaniumDetector *pGermanium = static_cast<germaniumDetector*>(drvPvt);
    pGermanium->udpDataThread();
}

//===========================================================================//

/*
 * UDP data reception thread - handles incoming data packets
 */
void germaniumDetector::udpDataThread()
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
                //processReceivedData(receiveBuffer, static_cast<size_t>(bytesReceived));
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
//extern "C"
void germaniumDetector::udpControlThreadC(void *drvPvt)
{
    germaniumDetector *pGermanium = static_cast<germaniumDetector*>(drvPvt);
    pGermanium->udpControlThread();
}

//===========================================================================//

/*
 * UDP control thread - handles control command responses and status updates
 */
void germaniumDetector::udpControlThread()
{
    printf("UDP control thread started\n");

    uint8_t receiveBuffer[1024]; // Smaller buffer for control messages
    struct sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);

    while (threadsRunning)
    {
        // Wait for control responses with timeout
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(udpControlSocket, &readfds);
        timeout.tv_sec = 1;   // 1 second timeout
        timeout.tv_usec = 0;

        int result = select(udpControlSocket + 1, &readfds, nullptr, nullptr, &timeout);

        if (result > 0 && FD_ISSET(udpControlSocket, &readfds))
        {
            // Receive control response
            ssize_t bytesReceived = recvfrom(udpControlSocket, receiveBuffer, sizeof(receiveBuffer), 0,
                                           (struct sockaddr*)&senderAddr, &senderAddrLen);

            if (bytesReceived > 0)
            {
                // Process control response
                processResponse(receiveBuffer, static_cast<size_t>(bytesReceived));
            }
            else if (bytesReceived < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                printf("Error receiving UDP control response: %s\n", strerror(errno));
            }
        }
        else if (result < 0 && errno != EINTR)
        {
            printf("Error in UDP control select: %s\n", strerror(errno));
        }

        // Brief pause to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    printf("UDP control thread stopped\n");
}

//===========================================================================//

