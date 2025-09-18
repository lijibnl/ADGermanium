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
//uint32_t germaniumDetector::packGlobalConfig(const globalstr_t& global)
//{
//    uint32_t packed = 0;
//    
//    // Pack bit fields according to MARS ASIC specification
//    // Note: Bit positions should match the actual hardware specification from Mars_DDM
//    packed |= packBits(global.st,   0,  3);   // Shaping time (3 bits)
//    packed |= packBits(global.g,    3,  3);   // Gain (3 bits)  
//    packed |= packBits(global.pol,  6,  1);   // Polarity (1 bit)
//    packed |= packBits(global.eblk, 7,  2);   // Bias current enable (2 bits)
//    packed |= packBits(global.gmon, 9,  1);   // Global monitor mode (1 bit)
//    packed |= packBits(global.puen, 10, 1);   // Pileup rejection enable (1 bit)
//    packed |= packBits(global.mfs,  11, 1);   // Multi-fire suppression (1 bit)
//    packed |= packBits(global.tds,  12, 2);   // TDC slope (2 bits)
//    packed |= packBits(global.tdm,  14, 2);   // TDC mode (2 bits)
//    packed |= packBits(global.th,   16, 10);  // Threshold (10 bits)
//    
//    return packed;
//}

//===========================================================================//

/*
 * Convert globalstr and channelstr configuration into loads[] array
 * This is the optimized version of the wrap() function from Mars_DDM
 * Based on the original Mars_DDM implementation
 */
//void germaniumDetector::wrapOptimized()
//{
//    // Clear loads array first
//    memset(loads, 0, sizeof(loads));
//
//    // Process each chip's configuration
//    for (int chip = 0; chip < nchips && chip < 12; chip++)
//    {
//        int reg_index = 0;
//
//        // Pack global configuration for this chip into a single 32-bit word
//        uint32_t global_config = packGlobalConfig(globalstr[chip]);
//
//        // Store global config in first register position for this chip
//        loads[chip][reg_index++] = global_config;
//
//        // Process channels for this chip (32 channels per chip in MARS ASIC)
//        int channels_per_chip = 32;
//        int start_channel = chip * channels_per_chip;
//
//        for (int ch = 0; ch < channels_per_chip && reg_index < 14; ch++)
//        {
//            int channel_index = start_channel + ch;
//            if (channel_index < nelm_)
//            {
//                // Pack channel configuration into 32-bit word
//                // Multiple channels can be packed into single 32-bit words for efficiency
////                uint32_t channel_config = packChannelConfig(channelstr[channel_index]);
////
////                // Store channel config (could pack multiple channels per word if needed)
////                loads[chip][reg_index++] = channel_config;
//            }
//            else
//            {
//                // Unused channel - store zero
//                loads[chip][reg_index++] = 0;
//            }
//        }
//
//        // Fill remaining register positions with zeros if needed
//        while (reg_index < 14)
//        {
//            loads[chip][reg_index++] = 0;
//        }
//    }
//
//    printf( "Germanium: Packed configuration into loads\n" );
//}

//===========================================================================//

/*/
 * Original wrap definition.
 */

void germaniumDetector::wrap()
{
	int chip, tmp, j, chn;
    
	unsigned int tword, tword2;
	for (chip = 0; chip < nchips_; chip++)
	{
		/* do globals first */

		//--------------------------------------------//

		j = 0;
		tword = 0;
		tword = globalstr[chip].tm & 1;
		/*1 */ j++;
		tword = tword << 1 | (globalstr[chip].sbm & 1);
		/*2 */ j++;
		tword = tword << 1 | (globalstr[chip].saux & 1);
		/*3 */ j++;
		tword = tword << 1 | (globalstr[chip].sp & 1);
		/*4 */ j++;
		tword = tword << 1 | (globalstr[chip].slh & 1);
		/*5 */ j++;
		tword = tword << 2 | (globalstr[chip].g & 3);
		/*7 */ j += 2;
		tword = tword << 5 | (globalstr[chip].c & 0x1f);
		/*12 */ j += 5;
		tword = tword << 2 | (globalstr[chip].ss & 3);
		/*14 */ j += 2;
		tword = tword << 2 | (globalstr[chip].tr & 3);
		/*16 */ j += 2;
		tword = tword << 1 | (globalstr[chip].sse & 1);
		/*17 */ j++;
		tword = tword << 1 | (globalstr[chip].spur & 1);
		/*18 */ j++;
		tword = tword << 1 | (globalstr[chip].rt & 1);
		/*19 */ j++;
		tword = tword << 2 | (globalstr[chip].ts & 3);
		/*21 */ j += 2;
		tword = tword << 1 | (globalstr[chip].sl & 1);
		/*22 */ j++;
		tword = tword << 1 | (globalstr[chip].sb & 1);
		/*23 */ j++;
		tword = tword << 1 | (globalstr[chip].sbn & 1);
		/*24 */ j++;
		tword = tword << 1 | (globalstr[chip].m1 & 1);
		/*25 */ j++;
		tword = tword << 1 | (globalstr[chip].m0 & 1);
		/*26 */ j++;
		tword = tword << 1 | (globalstr[chip].senfl2 & 1);
		/*27 */ j++;
		tword = tword << 1 | (globalstr[chip].senfl1 & 1);
		/*28 */ j++;
		tword = tword << 1 | (globalstr[chip].rm & 1);
		/*29 */ j++;
		tmp = globalstr[chip].pb;
		tword = tword << 3 | ((tmp >> 7) & 7);
		/*32 */ j += 3;

		loads[chip][0] = tword;

		//--------------------------------------------//

		tword2 = 0;
		j = 0;
		tword2 = tword2 | (tmp & 0x7f);
		/*7 */ j += 7;
		tword2 = tword2 << 10 | (globalstr[chip].pa & 0x3ff);
		/*17 */ j += 10;

		chn = 31;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*18 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*19 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*20 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*21 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*24 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*25 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*29 */ j += 4;

		chn = 30;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*30 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*31 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*32 */ j++;

		loads[chip][1] = tword2;

		//--------------------------------------------//

		tword2 = 0;

		j = 0;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*1 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*4 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*5 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*9 */ j += 4;

		chn = 29;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*10 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*11 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*12 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*13 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*16 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*17 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*21 */ j += 4;

		chn = 28;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*22 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*23 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*24 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*25 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*28 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*29 */ j++;
		tmp = (channelstr[chip * 32 + chn].dp & 15);
		tword2 = tword2 << 3 | ((tmp >> 1) & 7);
		/*32 */ j += 3;

		loads[chip][2] = tword2;

		//--------------------------------------------//

		tword2 = 0;
		j = 0;
		tword2 = tword2 | (tmp & 0x1);
		/*1 */ j++;
		chn = 27;

		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*2 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*3 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*4 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*5 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*8 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*9 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*13 */ j += 4;

		chn = 26;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*14 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*15 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*16 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*17 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*20 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*21 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*25 */ j += 4;

		chn = 25;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*26 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*27 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*28 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*29 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*32 */ j += 3;

		loads[chip][3] = tword2;

		//--------------------------------------------//

		j = 0;

		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*1 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*5 */ j += 4;

		chn = 24;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*6 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*7 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*8 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*9 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*12 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*13 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*17 */ j += 4;

		chn = 23;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*18 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*19 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*20 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*21 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*24 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*25 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*29 */ j += 4;

		chn = 22;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*30 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*31 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*32 */ j++;

		loads[chip][4] = tword2;

		tword2 = 0;
		j = 0;

		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*1 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*4 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*5 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*9 */ j += 4;

		chn = 21;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*10 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*11 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*12 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*13 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*16 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*17 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*21 */ j += 4;

		chn = 20;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*22 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*23 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*24 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*25 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*28 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*29 */ j++;
		tmp = (channelstr[chip * 32 + chn].dp & 15);
		tword2 = tword2 << 3 | (tmp >> 1);
		/*32 */ j += 3;

		loads[chip][5] = tword2;

		//--------------------------------------------//
		tword2 = 0;
		j = 0;
		tword2 = tword2 | (tmp & 1);
		/*1 */ j++;
		chn = 19;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*2 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*3 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*4 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*5 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*8 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*9 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*13 */ j += 4;

		chn = 18;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*14 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*15 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*16 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*17 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*20 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*21 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*25 */ j += 4;

		chn = 17;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*26 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*27 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*28 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*29 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*32 */ j += 3;

		loads[chip][6] = tword2;

		//--------------------------------------------//

		tword2 = 0;
		j = 0;

		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*1 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*5 */ j += 4;

		chn = 16;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*6 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*7 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*8 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*9 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*12 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*13 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*17 */ j += 4;

		chn = 15;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*18 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*19 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*20 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*21 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*24 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*25 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*29 */ j += 4;

		chn = 14;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*30 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*31 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*32 */ j++;

		loads[chip][7] = tword2;

		//--------------------------------------------//

		tword2 = 0;
		j = 0;

		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*1 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*4 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*5 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*9 */ j += 4;

		chn = 13;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*10 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*11 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*12 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*13 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*16 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*17 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*21 */ j += 4;

		chn = 12;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*22 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*23 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*24 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*25 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*28 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*29 */ j++;
		tmp = (channelstr[chip * 32 + chn].dp & 15);
		tword2 = tword2 << 3 | (tmp >> 1);
		/*32 */ j += 3;

		loads[chip][8] = tword2;

		//--------------------------------------------//

		tword2 = 0;
		j = 0;

		tword2 = tword2 | (tmp & 1);
		/*1 */ j += 1;

		chn = 11;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*2 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*3 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*4 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*5 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*8 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*9 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*13 */ j += 4;

		chn = 10;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*14 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*15 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*16 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*17 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*20 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*21 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*25 */ j += 4;

		chn = 9;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*26 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*27 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*28 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*29 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*32 */ j += 3;

		loads[chip][9] = tword2;

		//--------------------------------------------//

		tword2 = 0;
		j = 0;

		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*1 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*5 */ j += 4;

		chn = 8;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*6 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*7 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*8 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*9 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*12 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*13 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*17 */ j += 4;

		chn = 7;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*18 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*19 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*20 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*21 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*24 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*25 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*29 */ j += 4;

		chn = 6;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*30 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*31 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*32 */ j++;

		loads[chip][10] = tword2;

		//--------------------------------------------//

		tword2 = 0;
		j = 0;

		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*1 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*4 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*5 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*9 */ j += 4;

		chn = 5;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*10 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*11 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*12 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*13 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*16 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*17 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*21 */ j += 4;

		chn = 4;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*22 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*23 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*24 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*25 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*28 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*29 */ j++;
		tmp = (channelstr[chip * 32 + chn].dp & 15);
		tword2 = tword2 << 3 | (tmp >> 1);
		/*32 */ j += 3;

		loads[chip][11] = tword2;

		//--------------------------------------------//

		tword2 = 0;
		j = 0;

		tword2 = tword2 | (tmp & 1);
		/*1 */ j++;
		chn = 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*2 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*3 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*4 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*5 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*8 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*9 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*13 */ j += 4;

		chn = 2;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*14 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*15 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*16 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*17 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*20 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*21 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*25 */ j += 4;

		chn = 1;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*26 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*27 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*28 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*29 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*32 */ j += 3;

		loads[chip][12] = tword2;

		//--------------------------------------------//

		tword2 = 0;
		j = 0;

		tword2 = tword2 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*1 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*5 */ j += 4;

		chn = 0;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].st & 1);
		/*6 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sm & 1);
		/*7 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc2 & 1);
		/*8 */ j++;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].sel & 1);
		/*9 */ j++;
		tword2 = tword2 << 3 | (channelstr[chip * 32 + chn].da & 7);
		/*12 */ j += 3;
		tword2 = tword2 << 1 | (channelstr[chip * 32 + chn].nc1 & 1);
		/*13 */ j++;
		tword2 = tword2 << 4 | (channelstr[chip * 32 + chn].dp & 15);
		/*17 */ j += 4;
		tword2 = tword2 << 15;
		/*32 */ j += 15;

		loads[chip][13] = tword2;

		//--------------------------------------------//
	}
}



//===========================================================================//

/*
 * Bit field wrapping using bit field structures (alternative implementation)
 */
void germaniumDetector::wrapBitFields()
{
    // Alternative implementation using bit field structures
    // This is kept for compatibility but wrapOptimized() is preferred
    //wrapOptimized();
    wrap();
}

//===========================================================================//

/*
 * Validate MARS configuration before sending to hardware
 */
//void germaniumDetector::validateConfiguration()
//{
//    bool valid = true;
//
//    // Check global configuration for each chip
//    for (int chip = 0; chip < nchips_; chip++)
//    {
//        if (globalstr[chip].st < 0 || globalstr[chip].st > 7)
//        {
//            printf("Germanium: Invalid shaping time %d for chip %d\n", globalstr[chip].st, chip);
//            valid = false;
//        }
//
//        if (globalstr[chip].g < 0 || globalstr[chip].g > 7)
//        {
//            printf("Germanium: Invalid gain %d for chip %d\n", globalstr[chip].g, chip);
//            valid = false;
//        }
//
//        if (globalstr[chip].th < 0 || globalstr[chip].th > 1023)
//        {
//            printf("Germanium: Invalid threshold %d for chip %d\n", globalstr[chip].th, chip);
//            valid = false;
//        }
//    }
//
//    // Check channel configuration
//    for (int channel = 0; channel < nelm_; channel++)
//    {
//        if (channelstr[channel].thtr < 0 || channelstr[channel].thtr > 15)
//        {
//            printf("Germanium: Invalid threshold trim %d for channel %d\n", channelstr[channel].thtr, channel);
//            valid = false;
//        }
//
//        if (channelstr[channel].putr < 0 || channelstr[channel].putr > 15)
//        {
//            printf("Germanium: Invalid pileup trim %d for channel %d\n", channelstr[channel].putr, channel);
//            valid = false;
//        }
//    }
//
//    if (!valid)
//    {
//        printf("Germanium: Configuration validation failed!\n");
//    }
//}
//
//===========================================================================//

/*
 * Update loads array from current globalstr and channelstr configuration
 * This should be called whenever configuration parameters change
 */
void germaniumDetector::updateLoadsArray()
{
    //validateConfiguration();
    wrapBitFields();
}

//===========================================================================//

/*
 * Initialize MARS ASIC configuration - implement exact zDDM logic
 * This matches the original initMars() function
 */
void germaniumDetector::initializeMarsConfig()
{
    // Initialize global configuration for all chips
//    for (int chip = 0; chip < nchips; chip++)
//    {
//        globalstr[chip].st = 1;    // Default shaping time (0.25us)
//        globalstr[chip].g = 0;     // Default gain (240keV)
//        globalstr[chip].pol = 1;   // Positive polarity
//        globalstr[chip].eblk = 1;  // 2pA bias current
//        globalstr[chip].gmon = 0;  // Monitor off
//        globalstr[chip].puen = 0;  // Pileup rejection disabled
//        globalstr[chip].mfs = 0;   // Multi-fire suppression off
//        globalstr[chip].tds = 0;   // TDC slope 1us
//        globalstr[chip].tdm = 0;   // Time of arrival mode
//        globalstr[chip].th = 512;  // Default threshold
//    }
//
//    // Initialize channel configuration for all channels
//    for (int channel = 0; channel < nelm_; channel++)
//    {
//        channelstr[channel].chen = 1;  // Enable all channels by default
//        channelstr[channel].tsen = 0;  // Test pulse disabled
//        channelstr[channel].thtr = 0;  // No threshold trim
//        channelstr[channel].putr = 0;  // No pileup trim
//    }
//
//    // Generate initial loads array
//    updateLoadsArray();

//    for ( int i = 0; i < 12; i++ )
//    {   
//        globalstr[i].pa     = 1023;  /* Threshold dac */
//        globalstr[i].pb     = 0;     /* Test pulse dac */
//        globalstr[i].rm     = 1;     /* Readout mods; 1=synch, 0=asynch */
//        globalstr[i].senfl1 = 0;     /* Lock on peak found */
//        globalstr[i].senfl2 = 1;     /* Lock on threshold */
//        globalstr[i].m0     = 0;     /* 1=channel mon, 0=others */
//        globalstr[i].m1     = 1;     /* 1=pk det on PD/PN; 0=other mons on PD/PN */
//        globalstr[i].sbn    = 0;     /* enable buffer on pdn & mon outputs */
//        globalstr[i].sb     = 1;     /* enable buffer on pd & mon outputs */
//        globalstr[i].sl     = 0;     /* 0=internal 2pA leakage, 1=disabled */
//        globalstr[i].ts     = 1;     /* Shaping time */
//        globalstr[i].rt     = 0;     /* 1=timing ramp duration x 3 */
//        globalstr[i].spur   = 1;     /* 1=enable pileup rejector */
//        globalstr[i].sse    = 0;     /* 1=enable multiple-firing suppression */
//        globalstr[i].tr     = 1;     /* timing ramp adjust */
//        globalstr[i].ss     = 2;     /* multiple firing time adjust */
//        globalstr[i].c      = 31;    /* m0=0,Monitor select. m0=1, channel being monitored */
//        globalstr[i].g      = 1;     /* Gain select */
//        globalstr[i].slh    = 0;     /* internal leakage adjust */
//        globalstr[i].sp     = 1;     /* Input polarity; 1=positive, 0=negative */
//        globalstr[i].saux   = 0;     /* Enable monitor output */
//        globalstr[i].sbm    = 1;     /* Enable output monitor buffer */
//        globalstr[i].tm     = 0;     /* Timing mode; 0=ToA, 1=ToT */
//
//        for ( int j = 0; j < 32; j++ )
//        {   
//            channelstr[i * 32 + j].dp  = 15; /* Pileup rejector trim dac */
//            channelstr[i * 32 + j].nc1 = 0;  /* no connection, set 0 */
//            channelstr[i * 32 + j].da  = 5;  /* Threshold trim dac */
//            channelstr[i * 32 + j].sel = 1;  /* 1=leakage current, 0=shaper output */
//            channelstr[i * 32 + j].nc2 = 0;  /* no connection, set 0 */
//            channelstr[i * 32 + j].sm  = 1;  /* 1=channel disable */
//            channelstr[i * 32 + j].st  = 0;  /* 1=enable test input (30fF cap) */
//        }
//    }   


    printf("Germanium: Initialized MARS configuration for %d chips, %d elements\n"
            , nchips_
            , nelm_
            );
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

