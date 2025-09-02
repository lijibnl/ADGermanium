/**
 * @file germaniumDetectorHardware.cpp
 * @brief Hardware communication and MARS ASIC configuration functions.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "germaniumDetector.hpp"
#include "germaniumDetectorTypes.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <sys/select.h>

//===========================================================================//

/*
 * Pack global configuration structure into 32-bit word
 * Based on Mars_DDM bit field definitions and MARS ASIC specification
 */
uint32_t germaniumDetector::packGlobalConfig(const globalstr_t& global)
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

//===========================================================================//

/*
 * Convert globalstr and channelstr configuration into loads[] array
 * This is the optimized version of the wrap() function from Mars_DDM
 * Based on the original Mars_DDM implementation
 */
void germaniumDetector::wrapOptimized()
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
//                uint32_t channel_config = packChannelConfig(channelstr[channel_index]);
//
//                // Store channel config (could pack multiple channels per word if needed)
//                loads[chip][reg_index++] = channel_config;
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

//===========================================================================//

/*
 * Bit field wrapping using bit field structures (alternative implementation)
 */
void germaniumDetector::wrapBitFields()
{
    // Alternative implementation using bit field structures
    // This is kept for compatibility but wrapOptimized() is preferred
    wrapOptimized();
}

//===========================================================================//

/*
 * Validate MARS configuration before sending to hardware
 */
void germaniumDetector::validateConfiguration()
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

//===========================================================================//

/*
 * Update loads array from current globalstr and channelstr configuration
 * This should be called whenever configuration parameters change
 */
void germaniumDetector::updateLoadsArray()
{
    validateConfiguration();
    wrapOptimized();
}

//===========================================================================//

/*
 * Initialize MARS ASIC configuration - implement exact zDDM logic
 * This matches the original initMars() function
 */
void germaniumDetector::initializeMarsConfig()
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

//===========================================================================//

/*
 * Initialize Germanium hardware via UDP
 */
void germaniumDetector::initializeGermaniumHardware()
{
    printf("Germanium: Initializing hardware via UDP...\n");

    // Initialize MARS ASIC configuration first
    initializeMarsConfig();

    // Send initial configuration to device
    sendMarsConfiguration();
    //sendConfigurationToDevice();

    printf("Germanium: Hardware initialization complete\n");
}

//===========================================================================//

