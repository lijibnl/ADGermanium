/*
 * GermaniumHardware.cpp
 * Hardware communication and MARS ASIC configuration functions
 * UDP networking, register access, and MARS configuration management
 */

#include "Germanium.hpp"
#include "GermaniumTypes.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <sys/select.h>

/*
 * Pack global configuration structure into 32-bit word
 * Based on Mars_DDM bit field definitions and MARS ASIC specification
 */
uint32_t Germanium::packGlobalConfig(const globalstr_t& global)
{
    uint32_t packed = 0;
    
    // Pack bit fields according to MARS ASIC specification
    // Note: Bit positions should match the actual hardware specification from Mars_DDM
    packed |= packBits(global.st,   0,  3);   // Shaping time (3 bits)
    packed |= packBits(global.g,    3,  3);   // Gain (3 bits)  
    packed |= packBits(global.pol,  6,  1);   // Polarity (1 bit)
    packed |= packBits(global.eblk, 7,  2);   // Bias current enable (2 bits)
    packed |= packBits(global.gmon, 9,  1);   // Global monitor mode (1 bit)
    packed |= packBits(global.puen, 10, 1);   // Pileup rejection enable (1 bit)
    packed |= packBits(global.mfs,  11, 1);   // Multi-fire suppression (1 bit)
    packed |= packBits(global.tds,  12, 2);   // TDC slope (2 bits)
    packed |= packBits(global.tdm,  14, 2);   // TDC mode (2 bits)
    packed |= packBits(global.th,   16, 10);  // Threshold (10 bits)
    
    return packed;
}

/*
 * Convert globalstr and channelstr configuration into loads[] array
 * This is the optimized version of the wrap() function from Mars_DDM
 * Based on the original Mars_DDM implementation
 */
void Germanium::wrapOptimized()
{
    // Clear loads array first
    memset(loads, 0, sizeof(loads));

    // Process each chip's configuration
    for (int chip = 0; chip < nchips && chip < 12; chip++)
    {
        int reg_index = 0;

        // Pack global configuration for this chip into a single 32-bit word
        uint32_t global_config = packGlobalConfig(globalstr[chip]);

        // Store global config in first register position for this chip
        loads[chip][reg_index++] = global_config;

        // Process channels for this chip (32 channels per chip in MARS ASIC)
        int channels_per_chip = 32;
        int start_channel = chip * channels_per_chip;

        for (int ch = 0; ch < channels_per_chip && reg_index < 14; ch++)
        {
            int channel_index = start_channel + ch;
            if (channel_index < numElements)
            {
                // Pack channel configuration into 32-bit word
                // Multiple channels can be packed into single 32-bit words for efficiency
                uint32_t channel_config = packChannelConfig(channelstr[channel_index]);

                // Store channel config (could pack multiple channels per word if needed)
                loads[chip][reg_index++] = channel_config;
            }
            else
            {
                // Unused channel - store zero
                loads[chip][reg_index++] = 0;
            }
        }

        // Fill remaining register positions with zeros if needed
        while (reg_index < 14)
        {
            loads[chip][reg_index++] = 0;
        }
    }

    printf( "Germanium: Packed configuration into loads\n" );
}

/*
 * Bit field wrapping using bit field structures (alternative implementation)
 */
void Germanium::wrapBitFields()
{
    // Alternative implementation using bit field structures
    // This is kept for compatibility but wrapOptimized() is preferred
    wrapOptimized();
}

/*
 * Validate MARS configuration before sending to hardware
 */
void Germanium::validateConfiguration()
{
    bool valid = true;

    // Check global configuration for each chip
    for (int chip = 0; chip < nchips; chip++)
    {
        if (globalstr[chip].st < 0 || globalstr[chip].st > 7)
        {
            printf("Germanium: Invalid shaping time %d for chip %d\n", globalstr[chip].st, chip);
            valid = false;
        }

        if (globalstr[chip].g < 0 || globalstr[chip].g > 7)
        {
            printf("Germanium: Invalid gain %d for chip %d\n", globalstr[chip].g, chip);
            valid = false;
        }

        if (globalstr[chip].th < 0 || globalstr[chip].th > 1023)
        {
            printf("Germanium: Invalid threshold %d for chip %d\n", globalstr[chip].th, chip);
            valid = false;
        }
    }

    // Check channel configuration
    for (int channel = 0; channel < numElements; channel++)
    {
        if (channelstr[channel].thtr < 0 || channelstr[channel].thtr > 15)
        {
            printf("Germanium: Invalid threshold trim %d for channel %d\n", channelstr[channel].thtr, channel);
            valid = false;
        }

        if (channelstr[channel].putr < 0 || channelstr[channel].putr > 15)
        {
            printf("Germanium: Invalid pileup trim %d for channel %d\n", channelstr[channel].putr, channel);
            valid = false;
        }
    }

    if (!valid)
    {
        printf("Germanium: Configuration validation failed!\n");
    }
}

/*
 * Update loads array from current globalstr and channelstr configuration
 * This should be called whenever configuration parameters change
 */
void Germanium::updateLoadsArray()
{
    validateConfiguration();
    wrapOptimized();
}



/*
 * Initialize hardware communication and MARS ASIC configuration
 */
//void Germanium::initializeGermaniumHardware()
//{
//    printf("Initializing Germanium hardware communication...\n");
//    
//    // Initialize MARS ASIC configuration to default values
//    for (int chip = 0; chip < nchips; chip++)
//    {
//        // Set default global configuration
//        globalstr[chip].st   = 1;     // Shaping time: 0.25us
//        globalstr[chip].g    = 0;     // Gain: 240keV
//        globalstr[chip].pol  = 1;     // Polarity: Positive
//        globalstr[chip].eblk = 1;     // Bias current: 2pA
//        globalstr[chip].th   = 100;   // Default threshold
//        globalstr[chip].puen = 0;     // Pileup rejection: Disabled
//        globalstr[chip].mfs  = 0;     // Multi-fire suppression: Off
//        globalstr[chip].tds  = 0;     // TDC slope: 1us
//        globalstr[chip].tdm  = 0;     // TDC mode: Time of arrival
//        globalstr[chip].gmon = 0;     // Global monitor: Off
//        globalstr[chip].c    = 0;     // Monitor channel: 0
//        globalstr[chip].m0   = 0;     // Monitor mode: Off
//        globalstr[chip].saux = 0;     // Auxiliary select: Off
//        
//        // Initialize channel configuration for this chip
//        for (int chan = 0; chan < 64; chan++)
//        {
//            int globalChan = chip * 64 + chan;
//            if (globalChan < numElements)
//            {
//                channelstr[globalChan].chen = 1;    // Channel enabled
//                channelstr[globalChan].tsen = 0;    // Test pulse disabled
//                channelstr[globalChan].thtr = 0;    // Threshold trim: 0
//                channelstr[globalChan].putr = 0;    // Pileup threshold trim: 0
//            }
//        }
//    }
//
//    updateLoadsArray();
//    
//    printf( "MARS ASIC configuration initialized for %d chips, %d elements\n"
//          , nchips
//          , numElements
//          );
//}

/*
 * Initialize MARS ASIC configuration - implement exact zDDM logic
 * This matches the original initMars() function
 */
void Germanium::initializeMarsConfig()
{
    // Initialize global configuration for all chips
    for (int chip = 0; chip < nchips; chip++)
    {
        globalstr[chip].st = 1;    // Default shaping time (0.25us)
        globalstr[chip].g = 0;     // Default gain (240keV)
        globalstr[chip].pol = 1;   // Positive polarity
        globalstr[chip].eblk = 1;  // 2pA bias current
        globalstr[chip].gmon = 0;  // Monitor off
        globalstr[chip].puen = 0;  // Pileup rejection disabled
        globalstr[chip].mfs = 0;   // Multi-fire suppression off
        globalstr[chip].tds = 0;   // TDC slope 1us
        globalstr[chip].tdm = 0;   // Time of arrival mode
        globalstr[chip].th = 512;  // Default threshold
    }

    // Initialize channel configuration for all channels
    for (int channel = 0; channel < numElements; channel++)
    {
        channelstr[channel].chen = 1;  // Enable all channels by default
        channelstr[channel].tsen = 0;  // Test pulse disabled
        channelstr[channel].thtr = 0;  // No threshold trim
        channelstr[channel].putr = 0;  // No pileup trim
    }

    // Generate initial loads array
    updateLoadsArray();

    printf("Germanium: Initialized MARS configuration for %d chips, %d elements\n",
           nchips, numElements);
}


/*
 * Initialize Germanium hardware via UDP
 */
void Germanium::initializeGermaniumHardware()
{
    printf("Germanium: Initializing hardware via UDP...\n");

    // Initialize MARS ASIC configuration first
    initializeMarsConfig();

    // Send initial configuration to device
    sendConfigurationToDevice();

    printf("Germanium: Hardware initialization complete\n");
}



///*
// * UDP communication initialization
// */
//bool Germanium::initializeUDPSockets()
//{
//    printf("Initializing UDP sockets for %s...\n", ipAddress);
//    
//    // Create control socket
//    udpControlSocket = socket(AF_INET, SOCK_DGRAM, 0);
//    if (udpControlSocket < 0)
//    {
//        printf("Failed to create UDP control socket\n");
//        return false;
//    }
//    
//    // Create data socket
//    udpDataSocket = socket(AF_INET, SOCK_DGRAM, 0);
//    if (udpDataSocket < 0)
//    {
//        printf("Failed to create UDP data socket\n");
//        close(udpControlSocket);
//        return false;
//    }
//    
//    // Setup control address
//    memset(&controlAddr, 0, sizeof(controlAddr));
//    controlAddr.sin_family = AF_INET;
//    controlAddr.sin_port = htons(controlPort);
//    inet_pton(AF_INET, ipAddress, &controlAddr.sin_addr);
//    
//    // Setup data address
//    memset(&dataAddr, 0, sizeof(dataAddr));
//    dataAddr.sin_family = AF_INET;
//    dataAddr.sin_port = htons(dataPort);
//    inet_pton(AF_INET, ipAddress, &dataAddr.sin_addr);
//    
//    // Create mutex for UDP operations
//    udpMutex = epicsMutexCreate();
//    if (!udpMutex)
//    {
//        printf("Failed to create UDP mutex\n");
//        closeUDPSockets();
//        return false;
//    }
//    
//    // Create semaphore for data availability
//    dataAvailable = epicsEventCreate(epicsEventEmpty);
//    if (!dataAvailable)
//    {
//        printf("Failed to create data availability event\n");
//        closeUDPSockets();
//        return false;
//    }
//    
//    udpInitialized = true;
//    printf("UDP sockets initialized successfully\n");
//    return true;
//}
//
///*
// * Close UDP sockets and cleanup
// */
//void Germanium::closeUDPSockets()
//{
//    udpInitialized = false;
//    
//    if (udpControlSocket >= 0)
//    {
//        close(udpControlSocket);
//        udpControlSocket = -1;
//    }
//    
//    if (udpDataSocket >= 0)
//    {
//        close(udpDataSocket);
//        udpDataSocket = -1;
//    }
//    
//    if (udpMutex)
//    {
//        epicsMutexDestroy(udpMutex);
//        udpMutex = nullptr;
//    }
//    
//    if (dataAvailable)
//    {
//        epicsEventDestroy(dataAvailable);
//        dataAvailable = nullptr;
//    }
//    
//    printf("UDP sockets closed\n");
//}
//
///*
// * Write to a hardware register via UDP
// */
//asynStatus Germanium::udpRegisterWrite(uint32_t address, uint32_t value)
//{
//    if (!udpInitialized)
//    {
//        return asynError;
//    }
//    
//    // Create UDP packet for register write
//    UDPPacket packet;
//    packet.command = UDP_CMD_REGISTER_WRITE;
//    packet.address = address;
//    packet.data = value;
//    packet.length = sizeof(uint32_t);
//    
//    ssize_t sent = sendto(udpControlSocket, &packet, sizeof(packet), 0,
//                         (struct sockaddr*)&controlAddr, sizeof(controlAddr));
//    
//    if (sent != sizeof(packet))
//    {
//        printf("Germanium: Failed to send register write command\n");
//        return asynError;
//    }
//    
//    return asynSuccess;
//}
//
///*
// * Read from a hardware register via UDP
// */
//asynStatus Germanium::udpRegisterRead(uint32_t address, uint32_t *value)
//{
//    if (!udpInitialized || !value)
//    {
//        return asynError;
//    }
//    
//    // Create UDP packet for register read
//    UDPPacket packet;
//    packet.command = UDP_CMD_REGISTER_READ;
//    packet.address = address;
//    packet.data = 0;
//    packet.length = 0;
//    
//    ssize_t sent = sendto(udpControlSocket, &packet, sizeof(packet), 0,
//                         (struct sockaddr*)&controlAddr, sizeof(controlAddr));
//    
//    if (sent != sizeof(packet))
//    {
//        printf("Germanium: Failed to send register read command\n");
//        return asynError;
//    }
//    
//    // Wait for response (with timeout)
//    fd_set readfds;
//    struct timeval timeout;
//    FD_ZERO(&readfds);
//    FD_SET(udpControlSocket, &readfds);
//    timeout.tv_sec = 1;  // 1 second timeout
//    timeout.tv_usec = 0;
//    
//    int result = select(udpControlSocket + 1, &readfds, nullptr, nullptr, &timeout);
//    if (result <= 0)
//    {
//        printf("Germanium: Timeout waiting for register read response\n");
//        return asynError;
//    }
//    
//    // Receive response
//    UDPPacket response;
//    ssize_t received = recvfrom(udpControlSocket, &response, sizeof(response), 0,
//                               nullptr, nullptr);
//    
//    if (received != sizeof(response) || response.command != UDP_CMD_REGISTER_READ_RESPONSE)
//    {
//        printf("Germanium: Invalid register read response\n");
//        return asynError;
//    }
//    
//    *value = response.data;
//    return asynSuccess;
//}
//
///*
// * Send string command via UDP
// */
//asynStatus Germanium::udpSendString(uint32_t command, const char *str)
//{
//    if (!udpInitialized || !str)
//    {
//        return asynError;
//    }
//    
//    size_t strLen = strlen(str);
//    if (strLen >= UDP_MAX_STRING_SIZE)
//    {
//        printf("Germanium: String too long for UDP transmission\n");
//        return asynError;
//    }
//    
//    // Create UDP packet for string command
//    UDPStringPacket packet;
//    packet.command = command;
//    packet.length = strLen;
//    strncpy(packet.data, str, UDP_MAX_STRING_SIZE - 1);
//    packet.data[UDP_MAX_STRING_SIZE - 1] = '\0';
//    
//    ssize_t sent = sendto(udpControlSocket, &packet, sizeof(packet), 0,
//                         (struct sockaddr*)&controlAddr, sizeof(controlAddr));
//    
//    if (sent != sizeof(packet))
//    {
//        printf("Germanium: Failed to send string command\n");
//        return asynError;
//    }
//    
//    return asynSuccess;
//}
//
///*
// * Send integer array via UDP
// */
//asynStatus Germanium::udpWriteIntArray(uint32_t command, const void *data, 
//                                      size_t dataSize, uint32_t param)
//{
//    if (!udpInitialized || !data)
//    {
//        return asynError;
//    }
//    
//    if (dataSize > UDP_MAX_ARRAY_SIZE)
//    {
//        printf("Germanium: Array too large for UDP transmission\n");
//        return asynError;
//    }
//    
//    // Create UDP packet for array data
//    UDPArrayPacket packet;
//    packet.command = command;
//    packet.param = param;
//    packet.length = dataSize;
//    memcpy(packet.data, data, dataSize);
//    
//    ssize_t sent = sendto(udpControlSocket, &packet, sizeof(packet), 0,
//                         (struct sockaddr*)&controlAddr, sizeof(controlAddr));
//    
//    if (sent != sizeof(packet))
//    {
//        printf("Germanium: Failed to send array data\n");
//        return asynError;
//    }
//    
//    return asynSuccess;
//}
//
//
//asynStatus Germanium::udpSendLoads( uint16* loads, size_t count )
//{
//    if (!udpInitialized)
//    {
//        return asynError;
//    }
//
//    UdpReqMsg msg;
//    msg.id = UDP_REQ_MSG_ID;
//    msg.op = LOADS;
//    memcpy( &msg.payload, loads, count );
//
//    ssize_t sent = sendto(udpControlSocket, &packet, sizeof(packet), 0,
//                         (struct sockaddr*)&controlAddr, sizeof(controlAddr));
//    
//    if (sent != sizeof(packet))
//    {
//        printf("Germanium: Failed to send MARS global config for chip %d\n", chip);
//        return asynError;
//    }
//    
//    return asynSuccess;
//}
//
//
///*
// * Send MARS global configuration for a specific chip
// */
//asynStatus Germanium::udpSendMarsGlobal(int chip, globalstr_t* config)
//{
//    if (!udpInitialized || !config || chip < 0 || chip >= nchips)
//    {
//        return asynError;
//    }
//    
//    // Create UDP packet for MARS global config
//    UDPMarsGlobalPacket packet;
//    packet.command = UDP_CMD_MARS_GLOBAL;
//    packet.chip = chip;
//    packet.config = *config;
//    
//    ssize_t sent = sendto(udpControlSocket, &packet, sizeof(packet), 0,
//                         (struct sockaddr*)&controlAddr, sizeof(controlAddr));
//    
//    if (sent != sizeof(packet))
//    {
//        printf("Germanium: Failed to send MARS global config for chip %d\n", chip);
//        return asynError;
//    }
//    
//    return asynSuccess;
//}
//
///*
// * Send MARS channel configuration for all channels
// */
//asynStatus Germanium::udpSendMarsChannels()
//{
//    if (!udpInitialized)
//    {
//        return asynError;
//    }
//    
//    // Send channel configuration in chunks if needed
//    const size_t maxChannelsPerPacket = UDP_MAX_ARRAY_SIZE / sizeof(MarsChannelConfig);
//    size_t channelsSent = 0;
//    
//    while (channelsSent < numElements)
//    {
//        size_t channelsInPacket = std::min(maxChannelsPerPacket, 
//                                          (size_t)(numElements - channelsSent));
//        
//        UDPMarsChannelPacket packet;
//        packet.command = UDP_CMD_MARS_CHANNELS;
//        packet.startChannel = channelsSent;
//        packet.numChannels = channelsInPacket;
//        
//        memcpy(packet.channels, &channelstr[channelsSent], 
//               channelsInPacket * sizeof(MarsChannelConfig));
//        
//        ssize_t sent = sendto(udpControlSocket, &packet, sizeof(packet), 0,
//                             (struct sockaddr*)&controlAddr, sizeof(controlAddr));
//        
//        if (sent != sizeof(packet))
//        {
//            printf("Germanium: Failed to send MARS channel config\n");
//            return asynError;
//        }
//        
//        channelsSent += channelsInPacket;
//    }
//    
//    return asynSuccess;
//}
//
///*
// * Setup data acquisition system
// */
//void Germanium::setupDataAcquisition()
//{
//    printf("Setting up data acquisition system...\n");
//    
//    // Initialize acquisition parameters
//    acquisitionRunning = false;
//    evttot = 0;
//    framestat = 0;
//    
//    // Clear all spectrum data
//    for (size_t i = 0; i < mcaData.size(); i++)
//    {
//        std::fill(mcaData[i].begin(), mcaData[i].end(), 0);
//        std::fill(tdcData[i].begin(), tdcData[i].end(), 0);
//    }
//    
//    std::fill(countRates.begin(), countRates.end(), 0);
//    std::fill(totalCounts.begin(), totalCounts.end(), 0);
//    
//    printf("Data acquisition system initialized\n");
//}
