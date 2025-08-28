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

/*
 * Initialize UDP sockets for communication with Zynq device
 * Returns true on success, false on failure
 */
bool Germanium::initializeUDPSockets()
{
    // Initialize device address structure
    memset(&deviceAddr, 0, sizeof(deviceAddr));
    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_addr.s_addr = inet_addr(ipAddress);
    
    // Create control socket (for commands and status)
    udpControlSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpControlSocket < 0) {
        printf("Germanium: Failed to create control socket: %s\n", strerror(errno));
        return false;
    }
    
    // Create data socket (for photon event reception)
    udpDataSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpDataSocket < 0) {
        printf("Germanium: Failed to create data socket: %s\n", strerror(errno));
        close(udpControlSocket);
        return false;
    }
    
    // Set socket options for reuse and non-blocking
    int opt = 1;
    setsockopt(udpControlSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(udpDataSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind data socket to receive data packets
    struct sockaddr_in dataBindAddr;
    memset(&dataBindAddr, 0, sizeof(dataBindAddr));
    dataBindAddr.sin_family = AF_INET;
    dataBindAddr.sin_addr.s_addr = INADDR_ANY;
    dataBindAddr.sin_port = htons(dataPort);
    
    if (bind(udpDataSocket, (struct sockaddr*)&dataBindAddr, sizeof(dataBindAddr)) < 0) {
        printf("Germanium: Failed to bind data socket to port %d: %s\n", 
               dataPort, strerror(errno));
        close(udpControlSocket);
        close(udpDataSocket);
        return false;
    }
    
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

/*
 * Send UDP command to Zynq device using proper message format
 * Returns asynSuccess on success, asynError on failure
 */
asynStatus Germanium::sendUDPCommand(uint32_t command, uint32_t address, uint32_t data)
{
    if (!udpInitialized) {
        return asynError;
    }
    
    // Create proper UDP message structure
    UdpReqMsg msg;
    msg.id = 0x1234;  // Fixed ID for now - could be incremental
    
    // Determine if this is a read or write operation based on command
    if (command == UDP_CMD_REGISTER_READ) {
        msg.op = makeReadOp(address & 0xFFFF);
        msg.payload.single_word.data = 0;  // No data for read
    } else {
        msg.op = makeWriteOp(address & 0xFFFF);
        msg.payload.single_word.data = htonl(data);
    }
    
    // Set destination port for control commands
    deviceAddr.sin_port = htons(controlPort);
    
    // Send command with mutex protection
    epicsMutexLock(udpMutex);
    ssize_t sent = sendto(udpControlSocket, &msg, sizeof(msg), 0,
                         (struct sockaddr*)&deviceAddr, sizeof(deviceAddr));
    epicsMutexUnlock(udpMutex);
    
    if (sent != sizeof(msg)) {
        printf("Germanium: Failed to send UDP command 0x%x: %s\n", 
               command, strerror(errno));
        return asynError;
    }
    
    return asynSuccess;
}

/*
 * UDP control receive thread - handles command responses and status updates
 */
void Germanium::udpControlReceiveThread()
{
    UdpRespMsg response;
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    
    printf("Germanium: UDP control receive thread started\n");
    
    while (threadsRunning) {
        // Use select for timeout to allow clean shutdown
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(udpControlSocket, &readfds);
        timeout.tv_sec = 1;  // 1 second timeout
        timeout.tv_usec = 0;
        
        int result = select(udpControlSocket + 1, &readfds, NULL, NULL, &timeout);
        
        if (result > 0 && FD_ISSET(udpControlSocket, &readfds)) {
            ssize_t received = recvfrom(udpControlSocket, &response, sizeof(response), 0,
                                      (struct sockaddr*)&fromAddr, &fromLen);
            
            if (received == sizeof(response)) {
                // Process response
                uint16_t id = response.id;
                uint16_t op = response.op;
                
                if (isReadOp(op)) {
                    // This is a read response
                    uint16_t address = getOpAddress(op);
                    uint32_t data = ntohl(response.payload.single_word.data);
                    
                    printf("Germanium: Read response - addr=0x%04X, data=0x%08X\n", address, data);
                    
                    // Update parameter readbacks based on address
                    // Map register addresses to PV parameters here
                    
                } else {
                    // This is a write confirmation
                    printf("Germanium: Write confirmation for op=0x%04X\n", op);
                }
                
                callParamCallbacks();
            }
        } else if (result < 0 && errno != EINTR) {
            printf("Germanium: Control socket select error: %s\n", strerror(errno));
            break;
        }
    }
    
    printf("Germanium: UDP control receive thread stopped\n");
}

/*
 * UDP data receive thread - handles photon event data packets
 */
void Germanium::udpDataReceiveThread()
{
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    
    printf("Germanium: UDP data receive thread started\n");
    
    while (threadsRunning) {
        // Use select for timeout
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(udpDataSocket, &readfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int result = select(udpDataSocket + 1, &readfds, NULL, NULL, &timeout);
        
        if (result > 0 && FD_ISSET(udpDataSocket, &readfds)) {
            ssize_t received = recvfrom(udpDataSocket, udpDataBuffer.get(), UDP_BUFFER_SIZE, 0,
                                      (struct sockaddr*)&fromAddr, &fromLen);
            
            if (received > 0) {
                dataBufferSize = received;
                // Signal data processing thread that new data is available
                epicsEventSignal(dataAvailable);
            }
        } else if (result < 0 && errno != EINTR) {
            printf("Germanium: Data socket select error: %s\n", strerror(errno));
            break;
        }
    }
    
    printf("Germanium: UDP data receive thread stopped\n");
}

/*
 * Data processing thread - processes received photon events and updates arrays
 */
void Germanium::dataProcessingThread()
{
    printf("Germanium: Data processing thread started\n");
    
    while (threadsRunning) {
        // Wait for data to be available (with timeout)
        if (epicsEventWaitWithTimeout(dataAvailable, 1.0) == epicsEventWaitOK) {
            // Process the data buffer
            size_t numEvents = dataBufferSize / sizeof(PhotonEvent);
            PhotonEvent *events = (PhotonEvent*)udpDataBuffer.get();
            
            for (size_t i = 0; i < numEvents; i++) {
                // Convert from network byte order
                uint16_t element = ntohs(events[i].element);
                uint16_t energy = ntohs(events[i].energy);
                uint32_t timestamp = ntohl(events[i].timestamp);
                
                // Validate element number
                if (element < numElements) {
                    processPhotonEvent(element, energy, timestamp);
                    totalCounts[element]++;
                    evttot++;
                }
            }
            
            // Update count rates periodically
            static int updateCounter = 0;
            if (++updateCounter % 1000 == 0) {
                updateCountRates();
                updateSpectra();
            }
        }
    }
    
    printf("Germanium: Data processing thread stopped\n");
}

/*
 * Static thread entry points for C compatibility
 */
void Germanium::udpControlThreadC(void *pPvt)
{
    Germanium *pGermanium = (Germanium*)pPvt;
    pGermanium->udpControlReceiveThread();
}

void Germanium::udpDataThreadC(void *pPvt)
{
    Germanium *pGermanium = (Germanium*)pPvt;
    pGermanium->udpDataReceiveThread();
}

void Germanium::dataProcessingThreadC(void *pPvt)
{
    Germanium *pGermanium = (Germanium*)pPvt;
    pGermanium->dataProcessingThread();
}

/*
 * UDP-based register write (replaces direct pl_register_write)
 */
asynStatus Germanium::udpRegisterWrite(uint32_t reg, uint32_t value)
{
    return sendUDPCommand(UDP_CMD_REGISTER_WRITE, reg, value);
}

/*
 * UDP-based register read (replaces direct pl_register_read)
 */
asynStatus Germanium::udpRegisterRead(uint32_t reg, uint32_t *value)
{
    // Send read command
    asynStatus status = sendUDPCommand(UDP_CMD_REGISTER_READ, reg, 0);
    
    if (status != asynSuccess) {
        return status;
    }
    
    // For now, return success - actual value would come via UDP response
    // In a real implementation, you'd wait for the response or use a callback
    *value = 0;
    return asynSuccess;
}

/*
 * FIFO reset via UDP command (replaces direct FIFO access)
 */
void Germanium::fifo_reset()
{
    sendUDPCommand(UDP_CMD_FIFO_RESET, 0, 0);
}

/*
 * FIFO disable via UDP command (replaces direct FIFO access)
 */
void Germanium::fifo_disable()
{
    sendUDPCommand(UDP_CMD_FIFO_DISABLE, 0, 0);
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
    
    // Copy loads array into the proper payload structure
    // The loads array is uint16_t[12*32] but payload expects uint32_t[12][14]
    // We need to pack the 16-bit values into 32-bit words
    for (int chip = 0; chip < 12; chip++) {
        for (int word = 0; word < 14; word++) {
            // Pack two 16-bit loads values into one 32-bit word
            int idx1 = chip * 32 + word * 2;
            int idx2 = chip * 32 + word * 2 + 1;
            
            uint32_t packed = 0;
            if (idx1 < nchips * 32) {
                packed |= (uint32_t)loads[idx1] << 16;
            }
            if (idx2 < nchips * 32) {
                packed |= (uint32_t)loads[idx2];
            }
            
            msg.payload.stuff_mars.loads[chip][word] = htonl(packed);
        }
    }
    
    // Set destination port for control commands  
    deviceAddr.sin_port = htons(controlPort);
    
    // Send the configuration data with mutex protection
    epicsMutexLock(udpMutex);
    ssize_t sent = sendto(udpControlSocket, &msg, sizeof(msg), 0,
                         (struct sockaddr*)&deviceAddr, sizeof(deviceAddr));
    epicsMutexUnlock(udpMutex);
    
    if (sent != sizeof(msg)) {
        printf("Germanium: Failed to send MARS configuration: %s\n", strerror(errno));
        return asynError;
    }
    
    printf("Germanium: Sent MARS configuration (%zu bytes) for %d chips\n", 
           sizeof(msg.payload.stuff_mars), nchips);
    return asynSuccess;
}

/*
 * Write integer array via UDP (generic function for array transfers)
 * This can be used for channel enable arrays, threshold arrays, etc.
 */
asynStatus Germanium::udpWriteIntArray(uint32_t command, const void *data, 
                                       size_t dataSize, uint32_t address)
{
    if (!udpInitialized) {
        return asynError;
    }
    
    if (dataSize > UDP_BUFFER_SIZE - sizeof(UDPCommand)) {
        printf("Germanium: Array data too large (%zu bytes) for UDP transfer\n", dataSize);
        return asynError;
    }
    
    // Create buffer for command + data
    uint8_t *buffer = new uint8_t[sizeof(UDPCommand) + dataSize];
    UDPCommand *header = (UDPCommand*)buffer;
    uint8_t *payload = buffer + sizeof(UDPCommand);
    
    // Fill header
    header->command = htonl(command);
    header->address = htonl(address);
    header->data = htonl(dataSize);
    header->checksum = htonl(command ^ address ^ dataSize);
    
    // Copy data
    memcpy(payload, data, dataSize);
    
    // Send with mutex protection
    deviceAddr.sin_port = htons(controlPort);
    
    epicsMutexLock(udpMutex);
    ssize_t sent = sendto(udpControlSocket, buffer, sizeof(UDPCommand) + dataSize, 0,
                         (struct sockaddr*)&deviceAddr, sizeof(deviceAddr));
    epicsMutexUnlock(udpMutex);
    
    delete[] buffer;
    
    if (sent != (ssize_t)(sizeof(UDPCommand) + dataSize)) {
        printf("Germanium: Failed to send array data (cmd=0x%x): %s\n", 
               command, strerror(errno));
        return asynError;
    }
    
    return asynSuccess;
}

/*
 * Send a string command via UDP
 */
asynStatus Germanium::udpSendString(uint32_t command, const char *str) {
    if (!udpInitialized || udpControlSocket < 0) {
        printf("Germanium: UDP not initialized for string command\n");
        return asynError;
    }
    
    // Create UDP command packet with string payload
    size_t strLen = strlen(str);
    size_t packetSize = sizeof(UDPCommand) + strLen + 1; // +1 for null terminator
    
    uint8_t *packet = new uint8_t[packetSize];
    UDPCommand *cmd = (UDPCommand*)packet;
    
    cmd->command = command;
    cmd->address = 0;
    cmd->data_length = strLen + 1;
    
    // Copy string after the command header
    strcpy((char*)(packet + sizeof(UDPCommand)), str);
    
    // Send packet
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(controlPort);
    inet_pton(AF_INET, ipAddress, &addr.sin_addr);
    
    ssize_t sent = sendto(udpControlSocket, packet, packetSize, 0,
                         (struct sockaddr*)&addr, sizeof(addr));
    
    delete[] packet;
    
    if (sent < 0) {
        printf("Germanium: Failed to send string command 0x%02X: %s\n", 
               command, strerror(errno));
        return asynError;
    }
    
    printf("Germanium: Sent string command 0x%02X: '%s'\n", command, str);
    return asynSuccess;
}
