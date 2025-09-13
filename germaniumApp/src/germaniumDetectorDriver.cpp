/**
 * @file germaniumDetectorDriver.cpp
 * @brief asynPortDriver interface implementations for germaniumDetector detector;
 *        handles all parameter read/write operations and communication with
 *        hardware.
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
#include "errlog.h"
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <arpa/inet.h>

//===========================================================================//

namespace { // internal linkage

[[nodiscard]] bool ipStrToU32Host(const char* s, uint32_t& outHost) noexcept {
    in_addr addr{};
    int rc = inet_pton(AF_INET, s, &addr);
    if (rc == 1) { outHost = ntohl(addr.s_addr); return true; }
    unsigned b1,b2,b3,b4;
    if (std::sscanf(s, "%u.%u.%u.%u", &b1,&b2,&b3,&b4) != 4) return false;
    if (b1>255||b2>255||b3>255||b4>255) return false;
    outHost = (b1<<24)|(b2<<16)|(b3<<8)|b4;
    return true;
}

//===========================================================================//

void u32HostToIpStr(uint32_t host, char* buf, size_t buflen) noexcept {
    in_addr a{}; a.s_addr = htonl(host);
    inet_ntop(AF_INET, &a, buf, (socklen_t)buflen); // buflen >= INET_ADDRSTRLEN
}

}

//===========================================================================//

// asynPortDriver virtual method implementations
asynStatus germaniumDetector::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    errlogPrintf( "[%s]: function is %d\n", __func__, function );

    // First set the parameter locally
    status = ADDriver::setIntegerParam(function, value);

    if (status != asynSuccess)
    {
        printf( "setIntegerParam failed with status %d\n", status );
        return status;
    }

    // Send the parameter change to the remote device via UDP
    // Most MARS ASIC parameters are stored in globalstr/channelstr arrays
    // and sent via bulk configuration, not direct register writes
    if ( function == GermaniumSHPT )
    {
        errlogPrintf( "Update shaping time\n" );
        // Shaping time - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].ts = value;
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumGAIN )
    {
        errlogPrintf( "Update gain\n" );
        // Gain setting - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].g = value;
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumPOL )
    {
        // Polarity - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].sp = value;
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumEBLK )
    {
        // Bias current - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < nchips; chip++)
        {
            globalstr[chip].eblk = value;
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumTHRSH )
    {
        // Threshold settings - update globalstr for specific chip
        int chipIndex;
        getIntegerParam(GermaniumCHIP, &chipIndex);
        if (chipIndex >= 0 && chipIndex < nchips)
        {
            globalstr[chipIndex].th = value;
            status = sendMarsConfiguration();
        }
        else
        {
            status = asynError;
            printf("Germanium: Invalid chip index %d for threshold\n", chipIndex);
        }
    }

    else if ( function == GermaniumGMON )
    {
        errlogPrintf( "Set global monitor mode to %d\n", value );

        for (int chip = 0; chip < 12/*nchips_*/; chip++)
        {
            switch(value)
            {
                case 0:
                    // All monitors off. M0=0, C0-C4=00000
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 1:
                    // All off except this chip temp. M0=0 C0-C4=00100
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 2:
                    // All off except this chip base M0=0 C0-C4=10100
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 3:
                    // All off except this chip thresh M0=0 C0-C4=01100
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 4:
                    // All off except this chip test pulse M0=0 C0-C4=11100
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 5:
                    // All off except this chip channel monitor M0=1; channel number to C0-C4
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
            }
        }
        
        // Set specific chip settings based on current chip selection
        int currentChip;
        getIntegerParam(GermaniumCHIP, &currentChip);
        if (currentChip >= 0 && currentChip < nchips) {
            switch(value)
            {
                case 1:
                    // Temperature
                    globalstr[currentChip].c = 4;
                    globalstr[currentChip].saux = 1;
                    break;
                case 2:
                    // Baseline
                    globalstr[currentChip].c = 5;
                    globalstr[currentChip].saux = 1;
                    break;
                case 3:
                    // Threshold
                    globalstr[currentChip].c = 6;
                    globalstr[currentChip].saux = 1;
                    break;
                case 4:
                    // Test pulse
                    globalstr[currentChip].c = 7;
                    globalstr[currentChip].saux = 1;
                    break;
                case 5:
                    // Channel monitor
                    int currentChan;
                    getIntegerParam(GermaniumCHAN, &currentChan);
                    globalstr[currentChip].c = currentChan;
                    globalstr[currentChip].m0 = 1;
                    globalstr[currentChip].saux = 1;
                    break;
            }
        }
        status = sendMarsConfiguration();
    
        }

    else if ( function == GermaniumPUEN )
    {
        // Pileup rejection enable - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].spur = value;
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumMFS )
    {
        // Multi-fire suppression - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].sse = value;
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumTDS )
    {
        errlogPrintf( "Set time detector slope\n" );
        switch (pscal->tds)
        {
            case 0:
                j = 0; 
                rt = 0; 
                break;
            case 1:
                j = 1; 
                rt = 0; 
                break;
            case 2:
                j = 2; 
                rt = 0; 
                break;
            case 3:
                j = 3; 
                rt = 0; 
                break;
            case 4:
                j = 1; 
                rt = 1; 
                break;
            case 5:
                j = 2; 
                rt = 1; 
                break;
            case 6:
                j = 3; 
                rt = 1; 
                break;
        }

        for (chip = 0; chip < 12/*nchips_*/; chip++)
        {
            globalstr[chip].tr = j; 
            globalstr[chip].rt = rt;
        }

        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumTDM )
    {
        // TDC mode - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].tm = value;
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumMONCH )
    {
        errlogPrintf( "Select to monitor channel %d\n", value );
        // Set CHAN field to the current chip * 64 + MONCH value
        
        chip = value / 32;
        chan = value % 32;

        for ( i = 0; i < 4096; i++ )
        {
            spct[i]  = mca[4096 * value + i];
            spctx[i] = (float)i * slp[value] + offs[value];
        }
        errlogPrintf( "SPCT and SPCTX updated\n" );

        if (gmon_ == 5)
        {
            for (chip = 0; chip < 12; chip++)
            {
                globalstr[chip].c = 0;
                globalstr[chip].m0 = 0;
                globalstr[chip].saux = 0;
            }
            globalstr[pscal->chip].c = pscal->chan;
            globalstr[pscal->chip].m0 = 1;
            globalstr[pscal->chip].saux = 1;
            channelstr[pscal->monch].sel = pscal->loao;
            status = sendMarsConfiguratoin();
        }

        //int currentChip;
        //getIntegerParam(GermaniumCHIP, &currentChip);
        //if (currentChip >= 0 && currentChip < nchips)
        //{
        //    int chanValue = currentChip * 64 + value;
        //    setIntegerParam(GermaniumCHAN, chanValue);
        //}
        //
        //// Update the spectrum if we're in global monitor mode 5 (channel monitor)
        //int gmonMode;
        //getIntegerParam(GermaniumGMON, &gmonMode);
        //if (gmonMode == 5)
        //{
        //    // Send configuration to update the hardware monitor channel
        //    if (currentChip >= 0 && currentChip < nchips)
        //    {
        //        globalstr[currentChip].c = value;
        //        globalstr[currentChip].m0 = 1;
        //        globalstr[currentChip].saux = 1;
        //        status = sendMarsConfiguration();
        //    }
        //}
    }

    else if ( function == GermaniumCHIP )
    {
        // Chip selection - implement exact zDDM logic
        // Update CHAN to be CHIP * 64 + (CHAN % 64)
        int currentChan;
        getIntegerParam(GermaniumCHAN, &currentChan);
        int newChan = value * 64 + (currentChan % 64);
        setIntegerParam(GermaniumCHAN, newChan);
        
        // If we're in global monitor mode 5, update the hardware
        int gmonMode;
        getIntegerParam(GermaniumGMON, &gmonMode);
        if (gmonMode == 5)
        {
            int monch;
            getIntegerParam(GermaniumMONCH, &monch);
            if (value >= 0 && value < nchips)
            {
                globalstr[value].c = monch;
                globalstr[value].m0 = 1;
                globalstr[value].saux = 1;
                status = sendMarsConfiguration();
            }
        }
    }

    else if ( function == GermaniumCHAN )
    {
        // Channel selection - implement exact zDDM logic
        // Extract chip and monitor channel from absolute channel
        int chip = value / 64;
        int monch = value % 64;
        
        // Update CHIP and MONCH fields
        setIntegerParam(GermaniumCHIP, chip);
        setIntegerParam(GermaniumMONCH, monch);
        
        // If we're in global monitor mode 5, update the hardware
        int gmonMode;
        getIntegerParam(GermaniumGMON, &gmonMode);
        if (gmonMode == 5 && chip >= 0 && chip < nchips)
        {
            globalstr[chip].c = monch;
            globalstr[chip].m0 = 1;
            globalstr[chip].saux = 1;
            status = sendMarsConfiguration();
        }
    }

    else if ( function == GermaniumNELM )
    {
        status = udpRegisterWrite( NELM, value );
    }

    else if ( function == GermaniumLOAO )
    {
        errlogPrintf( "Set channel monitor to leakage/pulse\n" );
        channelstr[monch_].sel = value;
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumTPAMP )
    {
        errlogPrintf( "Set test pulse amplitude\n" );
        for (chip = 0; chip < 12/*nchips_*/; chip++)
        {
            globalstr[chip].pb = value;
        }

        status = udpRegisterWrite( MARS_CALPULSE, value);
    }

    else if ( function == GermaniumTPFRQ )
    {
        errlogPrintf( "Set test pulse frequency\n" );
        status  = udpRegisterWrite( CALPULSE_RATE, 25000000 / value / 2);
        status |= udpRegisterWrite( CALPULSE_WIDTH, 25000000 / value / 2);
        status |= sendMarsConfiguration();
    }

    else if ( function == GermaniumTPCNT )
    {
        status  = udpRegisterWrite( CALPULSE_CNT, value);
        status |= sendMarsConfiguration();
    }

    else if ( function == GermaniumTPENB )
    {
        if ( value == 1 )
        {
            status  = udpRegisterWrite( MARS_CALPULSE, 0xFFF );
            status |= udpRegisterWrite( CALPULSE_MODE, 1 );
        }
        else
        {
            status  = udpRegisterWrite( MARS_CALPULSE, 0 );
            status |= udpRegisterWrite( CALPULSE_MODE, 0 );
        }
        status |= sendMarsConfiguration();

    }

//    case GermaniumCNTS)
//    {
//        // Counts status - read-only, update from hardware
//        // This should be updated by a periodic read task
//        status = asynSuccess; // Read-only parameter
//    }
//    case GermaniumTIMS)
//    {
//        // Timer status - read-only, update from hardware
//        // This should be updated by a periodic read task
//        status = asynSuccess; // Read-only parameter
//    }
    else if ( function == GermaniumRATE )
    {
        // Rate - read-only, computed from counts and time
        // This should be updated by a periodic read task
        status = asynSuccess; // Read-only parameter
    }

    else if ( function == GermaniumFSIZE )
    {
        // File size limit - validate and store
        if (value < 1) // Minimum 1MB
        {
            printf("Germanium: File size too small, setting to 1MB minimum\n");
            setIntegerParam(GermaniumFSIZE, 1);
        }
        else if (value > 1000) // Maximum 1TB
        {
            printf("Germanium: File size too large, setting to 1TB maximum\n");
            setIntegerParam(GermaniumFSIZE, 1000);
        }
        printf("Germanium: Maximum file size set to %d MBytes\n", value);
    }

    else if ( function == GermaniumCNT )
    {
        // Acquisition control
        if (value == 1)
        { // Start acquisition
            startDataAcquisition();
        }
        else
        { // Stop acquisition
            stopDataAcquisition();
        }
    }
    
    else if ( function == GermaniumMODE )
    {
        status = udpRegisterWrite( COUNT_MODE, value);
    }

//    case GermaniumCLRE)
//    {
//        // Clear event - implement exact zDDM logic
//        if (value == 1) {
//            // Clear the event spectrum data
//            status = udpRegisterWrite( CLR_EVT, 1);
//            // Reset the value back to 0
//            setIntegerParam(GermaniumCLRE, 0);
//        }
//    }
//    case GermaniumCLRM)
//    {
//        // Clear monitor - implement exact zDDM logic
//        if (value == 1) {
//            // Clear the monitor spectrum data
//            status = udpRegisterWrite( CLR_MON, 1);
//            // Reset the value back to 0
//            setIntegerParam(GermaniumCLRM, 0);
//        }
//    }
//    case GermaniumCLRT)
//    {
//        // Clear timer - implement exact zDDM logic
//        if (value == 1) {
//            // Clear the timer
//            status = udpRegisterWrite( CLR_TIM, 1);
//            // Reset the value back to 0
//            setIntegerParam(GermaniumCLRT, 0);
//        }
//    }
//    case GermaniumSTRT)
//    {
//        // Start acquisition - implement exact zDDM logic
//        if (value == 1) {
//            // Start data acquisition
//            status = udpRegisterWrite( STRT, 1);
//            // Reset the value back to 0
//            setIntegerParam(GermaniumSTRT, 0);
//        }
//    }
//    case GermaniumSTOP)
//    {
//        // Stop acquisition - implement exact zDDM logic
//        if (value == 1) {
//            // Stop data acquisition
//            status = udpRegisterWrite( STOP, 1);
//            // Reset the value back to 0
//            setIntegerParam(GermaniumSTOP, 0);
//        }
//    }
    else if ( function == GermaniumPLDEL )
    {
        status = udpRegisterWrite( MARS_PIPE_DELAY, value);
    }

    else if ( function == GermaniumRODEL )
    {
        // Readout delay - may be part of MARS configuration
        status = udpRegisterWrite( MARS_RDOUT_ENB, value);
    }

    // Add more parameter mappings as needed
    else
    {
        printf( "Invalid function %d for writeInt32()\n", function );
        status = asynError;
    }
        
    if (status == asynSuccess)
    {
        callParamCallbacks();
    }
    else
    {
        printf("Germanium: Failed to send parameter %d to device\n", function);
    }

    return status;
}

//===========================================================================//

asynStatus germaniumDetector::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    errlogPrintf( "[%s]: function is %d\n", __func__, function );

    // First set the parameter locally
    status = ADDriver::setDoubleParam(function, value);

    if (status != asynSuccess)
    {
        printf( "setDoubleParam failed with status %d\n", status );
        return status;
    }

    // Send the parameter change to the remote device via UDP
    if ( function == GermaniumFREQ )
    {
        // Time base frequency - may not have direct register in GermaniumRegister.hpp
        // Could be derived from COUNT_TIME registers
        uint32_t regValue = (uint32_t)(value / 1000.0); // Example scaling
        status = udpRegisterWrite( COUNT_TIME_LO, regValue & 0xFFFF );
        if (status == asynSuccess)
        {
            status = udpRegisterWrite( COUNT_TIME_HI, (regValue >> 16) & 0xFFFF );
        }
    }

    else if ( function == GermaniumTP )
    {
        // Time preset - use COUNT_TIME registers
        uint32_t ticks = (uint32_t)(value * 1000000); // Convert seconds to microseconds
        status = udpRegisterWrite( COUNT_TIME_LO, ticks & 0xFFFF );
        if (status == asynSuccess)
        {
            status = udpRegisterWrite( COUNT_TIME_HI, (ticks >> 16) & 0xFFFF );
        }
    }

    else if ( function == GermaniumTP1 )
    {
        // Auto time preset (use same registers for now)
        uint32_t ticks = (uint32_t)(value * 1000000);
        status = udpRegisterWrite( COUNT_TIME_LO, ticks & 0xFFFF );
        if (status == asynSuccess)
        {
            status = udpRegisterWrite( COUNT_TIME_HI, (ticks >> 16) & 0xFFFF );
        }
    }

    else if ( function == GermaniumDLY )
    {
        // Delay setting - could be TD_CAL (Time Delay Calibration)
        uint32_t delayReg = (uint32_t)(value * 1000); // Convert to appropriate units
        status = udpRegisterWrite( TD_CAL, delayReg );
    }

    else if ( function == GermaniumRATE )
    {
        // Display rate - implement original zDDM bounds checking logic
        double rate = value;
        rate = std::min(60.0, std::max(0.0, rate)); // Same bounds as original
        if (rate != value) {
            // Update parameter with bounded value
            status = ADDriver::setDoubleParam(function, rate);
            printf("Germanium: RATE bounded to %f Hz\n", rate);
        }
        // RATE is primarily read-only display parameter, don't send to hardware
    }

    else
    {
        printf( "Invalid function %d for writeFloat64()\n", function );
        status = asynError;
    }

    if (status == asynSuccess)
    {
        callParamCallbacks();
    }
    else
    {
        printf("Germanium: Failed to send float parameter %d to device\n", function);
    }

    return status;
}

//===========================================================================//

asynStatus germaniumDetector::writeOctet(asynUser *pasynUser, const char *value,
                                 size_t maxChars, size_t *nActual)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    errlogPrintf( "[%s]: function is %d\n", __func__, function );

    // First set the parameter locally
    status = ADDriver::setStringParam(function, value);

    if (status != asynSuccess)
    {
        printf( "setDoubleParam failed with status %d\n", status );
        return status;
    }
    
    // Send string parameters to device if needed
    if ( function == GermaniumFNAM )
    {
        // Filename - might trigger file operations on remote device
//        status = udpSendString(UDP_CMD_SET_FILENAME, value);
    }

    else if ( function == GermaniumCALF )
    {
        // Calibration filename - load calibration on remote device
//        status = udpSendString(UDP_CMD_LOAD_CALIBRATION, value);

    }
    else if ( function == GermaniumDIR )
    {
        // Data directory - create if it doesn't exist
        createDataDirectory();
        printf("Germanium: Data directory set to %s\n", value);
    }
    
    else if ( function == GermaniumIPADDR )
    {
        printf("Germanium: IP address change to %s\n", value);

        uint32_t host;
        if (!ipStrToU32Host(value, host))
        {
            printf( "Invalid IP '%s'\n", value );
            return asynError; // leave RBV unchanged
        }

        uint32_t net = htonl(host);
        status = udpRegisterWrite( UDP_IP_ADDR, net );

        if (status != asynSuccess)
            return status;

        // Normalize what UIs see
        char norm[16];
        u32HostToIpStr(host, norm, sizeof(norm));
        setStringParam( GermaniumIPADDR, norm );
    }

    else
    {
        printf( "Invalid function %d for writeFloat64()\n", function );
        status = asynError;
    }

    if (status == asynSuccess)
    {
        callParamCallbacks();
    }
    else
    {
        printf("Germanium: Failed to send string parameter %d to device\n", function);
    }

    *nActual = strlen(value);

    return status;
}

//===========================================================================//

asynStatus germaniumDetector::readInt32( asynUser *pasynUser )
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    errlogPrintf( "[%s]: function is %d\n", __func__, function );

    if ( function == GermaniumVER )
    {
        status = udpRegisterRead( VERSIONREG );
    }

    else if ( function == GermaniumTEMP1 )
    {
        status = udpRegisterRead( TEMP1 );
    }

    else if ( function == GermaniumTEMP2 )
    {
        status = udpRegisterRead( TEMP2 );
    }

    else if ( function == GermaniumTEMP3 )
    {
        status = udpRegisterRead( TEMP3 );
    }

    else if ( function == GermaniumZTEMP )
    {
        status = udpRegisterRead( ZTEMP );
    }

    else if ( function == GermaniumHV )
    {
        status = udpRegisterRead( HV );
    }

    else if ( function == GermaniumHV_CURR )
    {
        status = udpRegisterRead( HV_CURR );
    }

    else
    {
        printf( "Invalid function %d for readInt32()\n", function );
        status = asynError;
    }

    if (status == asynSuccess)
    {
        callParamCallbacks();
    }
    else
    {
        printf("Germanium: Failed to read int32 parameter %d from device\n", function);
    }

    return status;
}

//===========================================================================//

asynStatus germaniumDetector::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                     size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    errlogPrintf( "[%s]: function is %d\n", __func__, function );

    // Handle array reads for MCA, TDC, etc. using vectors
    if ( function == GermaniumMCA )
    {
        // Return MCA data for selected element (defaulting to element 0)
        size_t elementIndex = 0; // Could be made configurable
        if (elementIndex < mcaData.size() && !mcaData[elementIndex].empty())
        {
            size_t copySize = std::min(nElements, mcaData[elementIndex].size());
            std::copy(mcaData[elementIndex].begin(),
                      mcaData[elementIndex].begin() + copySize,
                      value);
            *nIn = copySize;
        }
        else
        {
            memset(value, 0, nElements * sizeof(epicsInt32));
            *nIn = nElements;
        }
    }

    else if ( function == GermaniumTDC )
    {
        // Return TDC data for selected element
        size_t elementIndex = 0; // Could be made configurable
        if (elementIndex < tdcData.size() && !tdcData[elementIndex].empty())
        {
            size_t copySize = std::min(nElements, tdcData[elementIndex].size());
            std::copy(tdcData[elementIndex].begin(),
                      tdcData[elementIndex].begin() + copySize,
                      value);
            *nIn = copySize;
        }
        else
        {
            memset(value, 0, nElements * sizeof(epicsInt32));
            *nIn = nElements;
        }
    }

    else if ( ( function == GermaniumSPCT ) || ( function == GermaniumINTENS ) )
    {
        // Return count rates or other computed arrays
        if (!countRates.empty())
        {
            size_t copySize = std::min(nElements, countRates.size());
            std::copy(countRates.begin(), countRates.begin() + copySize, value);
            *nIn = copySize;
        }
        else
        {
            memset(value, 0, nElements * sizeof(epicsInt32));
            *nIn = nElements;
        }
    }

    else
    {
        printf( "Invalid function %d for readInt32Array()\n", function );
        status = asynError;
    }
    
    return status;
}

//===========================================================================//

asynStatus germaniumDetector::writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                      size_t nElements)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    errlogPrintf( "[%s]: function is %d\n", __func__, function );

    // Handle array writes for configuration arrays via UDP
    if ( function == GermaniumCHEN )
    {
        errlogPrintf( "Set channel enabln" );
        uint8_t *chenArray = new uint8_t[nElements];
        for (size_t chan = 0; chan < nelm_; chan++)
        {
            channelstr[chan].sm = value[chan];
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumPUTR )
    {
        for ( chan = 0; chan < nelm_; chan++ )
        {
            channelstr[chan].dp = value[chan];
            status = sendMarsConfiguration();
        }
    }

    else if ( function == GermaniumTHRSH )
    {
        for ( chip = 0; chip < 12/*nchips_*/; chip++ )
        {
            globalstr[chip].pa = value[chip];
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumTSEN )
    {
        for ( chan = 0; chan < nelm_; chan++ )
        {
            channelstr[chan].str = value[chan];
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumSLP || function == GermaniumOFFS )
    {
        // Calibration arrays - send as is
//        status = udpWriteIntArray(UDP_CMD_CALIBRATION_ARRAY, value,
//                                  nElements * sizeof(epicsInt32), function);
    }

    else
    {
        // Unknown array parameter
        status = asynError;
        printf("Germanium: Unknown array parameter %d in writeInt32Array\n", function);
    }

    if (status == asynSuccess)
    {
        callParamCallbacks();
    }

    return status;
}

//===========================================================================//

// ADDriver virtual method implementations
asynStatus germaniumDetector::readNDArray(asynUser *pasynUser, epicsInt32 *value,
                                  size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    size_t dims[2];
    NDArray *pArray = nullptr;

    errlogPrintf( "[%s]: function is %d\n", __func__, function );

    // Determine which array is being requested
    if ( function == GermaniumMCA )
    {
        // Create 2D array: [numElements x SPECTRUM_SIZE]
        dims[0] = numElements;
        dims[1] = SPECTRUM_SIZE;

        pArray = this->pNDArrayPool->alloc(2, dims, NDInt32, 0, nullptr);
        if (pArray)
        {
            epicsInt32 *pData = (epicsInt32*)pArray->pData;

            // Copy MCA data from all elements
            for (int elem = 0; elem < numElements; elem++)
            {
                for (int bin = 0; bin < SPECTRUM_SIZE; bin++)
                {
                    pData[elem * SPECTRUM_SIZE + bin] = mcaData[elem][bin];
                }
            }

            // Set NDArray attributes
            this->getAttributes(pArray->pAttributeList);

            // Do callbacks to registered clients
            doCallbacksGenericPointer(pArray, NDArrayData, 0);

            *nIn = dims[0] * dims[1];
        }
        else
        {
            status = asynError;
            printf("Germanium: Failed to allocate NDArray for MCA data\n");
        }
    }

    else if ( function == GermaniumTDC )
    {
        // Create 2D array: [numElements x TDC_SIZE]
        dims[0] = numElements;
        dims[1] = TDC_SIZE;

        pArray = this->pNDArrayPool->alloc(2, dims, NDInt32, 0, nullptr);
        if (pArray)
        {
            epicsInt32 *pData = (epicsInt32*)pArray->pData;

            // Copy TDC data from all elements
            for (int elem = 0; elem < numElements; elem++)
            {
                for (int bin = 0; bin < TDC_SIZE; bin++)
                {
                    pData[elem * TDC_SIZE + bin] = tdcData[elem][bin];
                }
            }

            this->getAttributes(pArray->pAttributeList);
            doCallbacksGenericPointer(pArray, NDArrayData, 0);

            *nIn = dims[0] * dims[1];
        }
        else
        {
            status = asynError;
            printf("Germanium: Failed to allocate NDArray for TDC data\n");
        }
    }

    else if ( function == GermaniumINTENS )
    {
        // Create 1D array: [numElements] - intensity per element
        dims[0] = numElements;

        pArray = this->pNDArrayPool->alloc(1, dims, NDInt32, 0, nullptr);
        if (pArray)
        {
            epicsInt32 *pData = (epicsInt32*)pArray->pData;

            // Copy intensity data (total counts per element)
            for (int elem = 0; elem < numElements; elem++)
            {
                pData[elem] = totalCounts[elem];
            }

            this->getAttributes(pArray->pAttributeList);
            doCallbacksGenericPointer(pArray, NDArrayData, 0);

            *nIn = dims[0];
        }
        else
        {
            status = asynError;
            printf("Germanium: Failed to allocate NDArray for intensity data\n");
        }
    }

    else if ( function == GermaniumSPCT )
    {
        // Single channel spectrum - 1D array [SPECTRUM_SIZE]
        int selectedElement;
        getIntegerParam(GermaniumCHAN, &selectedElement);
        selectedElement = selectedElement % numElements; // Ensure valid range

        dims[0] = SPECTRUM_SIZE;

        pArray = this->pNDArrayPool->alloc(1, dims, NDInt32, 0, nullptr);
        if (pArray)
        {
            epicsInt32 *pData = (epicsInt32*)pArray->pData;

            // Copy spectrum data for selected element
            for (int bin = 0; bin < SPECTRUM_SIZE; bin++)
            {
                pData[bin] = mcaData[selectedElement][bin];
            }

            this->getAttributes(pArray->pAttributeList);
            doCallbacksGenericPointer(pArray, NDArrayData, 0);

            *nIn = dims[0];
        }
        else
        {
            status = asynError;
            printf("Germanium: Failed to allocate NDArray for spectrum data\n");
        }
    }
    
    else
    {
        // Unknown array type
        status = asynError;
        printf("Germanium: Unknown array parameter %d in readNDArray\n", function);
        *nIn = 0;
    }
    

    // Release NDArray reference
    if (pArray)
    {
        pArray->release();
    }

    return status;
}

//===========================================================================//

/*
 * Process control response message from hardware
 */
void germaniumDetector::processResponse(const uint8_t* data, size_t dataSize)
{
    if (!data || dataSize < sizeof(UdpRespMsg))
    {
        return;
    }

    const UdpRespMsg* response = reinterpret_cast<const UdpRespMsg*>(data);

    // Verify this is a response message
    if ((response->op & 0x8000) == 0)
    {
        printf("Received non-response message on control channel\n");
        return;
    }

    // Extract register address and operation type
    uint16_t reg = response->op & 0x7FFF;
    bool isRead = (response->op & 0x4000) != 0;
    uint32_t val = response->payload.single_word.data;

    // Probably ZynqDetector should send readback right after register write

    switch ( reg )
    {
        //----------------------------------------------//
        case VERSIONREG:
            setIntegerParam( GermaniumVER, val );
            break;
        //----------------------------------------------//
        case MARS_CALPULSE:
            setIntegerParam( GermaniumTPAMP_RBV, val);
            break;
        //----------------------------------------------//
        case CALPULSE_RATE:
            setIntegerParam( GermaniumTPFRQ_RBV, val);
            break;
        //----------------------------------------------//
        case CALPULSE_CNT:
            setIntegerParam( GermaniumTPCNT_RBV, val);
            break;
        //----------------------------------------------//
        case CALPULSE_MODE:
            setIntegerParam( GermaniumTPENB_RBV, val);
            break;
        //----------------------------------------------//
        case TRIG:
            setIntegerParam( ADAcquire, val);
            break;
        //----------------------------------------------//
        case MARS_PIPE_DELAY:
            setIntegerParam( GermaniumPLDEL_RBV, val);
            break;
        //----------------------------------------------//
        case MARS_RDOUT_ENB:
            setIntegerParam( GermaniumRODEL_RBV, val);
            break;
        //----------------------------------------------//
        case DETECTOR_TYPE:
            setIntegerParam( GermaniumDETTYPE, val);
            break;
        //----------------------------------------------//
        case UDP_IP_ADDR:
        {
            uint32_t host = ntohl(val);
            char s[16];
            u32HostToIpStr( host, s, sizeof(s) );
            setStringParam( GermaniumIPADDR_RBV, s );
            break;
        }
        //----------------------------------------------//
        case TEMP1:
            setDoubleParam( GermaniumTEMP1, val);
            break;
        //----------------------------------------------//
        case TEMP2:
            setDoubleParam( GermaniumTEMP2, val);
            break;
        //----------------------------------------------//
        case TEMP3:
            setDoubleParam( GermaniumTEMP3, val);
            break;
        //----------------------------------------------//
        case ZTEMP:
            setDoubleParam( GermaniumZTEMP, val);
            break;
        //----------------------------------------------//
        case HV:
            setDoubleParam( GermaniumHV, val);
            break;
        //----------------------------------------------//
        case HV_RBV:
            setDoubleParam( GermaniumHV_RBV, val);
            break;
        //----------------------------------------------//
        case HV_CURR:
            setDoubleParam( GermaniumHV_CURR, val);
            break;
        //----------------------------------------------//
        //case :
        //    setIntegerParam( Germanium_RBV, val);
        //    break;
        //----------------------------------------------//
        default:
            printf( "Value received for unknown register %d\n", reg );
    }

    callParamCallbacks();

}

//===========================================================================//

void germaniumDetector::report(FILE *fp, int details)
{
    fprintf(fp, "Germanium detector: %d elements, %d chips\n", numElements, nchips);
    fprintf(fp, "IP Address: %s\n", ipAddress);

    if (details > 1)
    {
//        fprintf(fp, "Device FD: %d\n", device_fd);
        fprintf(fp, "Acquisition running: %s\n", acquisitionRunning ? "Yes" : "No");
    }

    // Call base class report
    ADDriver::report(fp, details);
}

//===========================================================================//

asynStatus germaniumDetector::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                    const char **pptypeName, size_t *psize)
{
    // Handle driver-specific parameter creation
    return ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
}

//===========================================================================//

