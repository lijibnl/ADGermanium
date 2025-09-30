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

//namespace { // internal linkage

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
        errlogPrintf( "setIntegerParam failed with status %d\n", status );
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

        if ( status == asynSuccess )
            setIntegerParam( GermaniumSHPT, value );
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
        
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumGAIN, value );
    }

    else if ( function == GermaniumPOL )
    {
        // Polarity - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].sp = value;
        }
        status = sendMarsConfiguration();
        
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumPOL, value );
    }

    else if ( function == GermaniumEBLK )
    {
        // Bias current - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips_*/; chip++)
        {
            switch( value )
            {
                case 0:
                    globalstr[chip].sl = 1;
                    break;
                case 1:
                    globalstr[chip].sl = 0;
                    break;
                case 2:
                    globalstr[chip].sl  = 0;
                    globalstr[chip].slh = 1;
                    break;
            }
        }
        status = sendMarsConfiguration();
        
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumEBLK, value );
    }

    else if ( function == GermaniumGMON )
    {
        errlogPrintf( "Set global monitor mode to %d\n", value );

        int gmon, monch, chip;
        getIntegerParam( GermaniumGMON, &gmon );
        getIntegerParam( GermaniumMONCH, &monch );
        chip = monch / 32;

        if (gmon == 0)
        { /* All monitors off. M0=0, C0-C4=00000*/
            for (int chip_set = 0; chip_set < 12; chip_set++)
            {
                globalstr[chip_set].c = 0;
                globalstr[chip_set].m0 = 0;
                globalstr[chip_set].saux = 0;
            }
        }
        if (gmon == 1)
        { /* All off except this chip_set temp. M0=0 C0-C4=00100*/
            for (int chip_set = 0; chip_set < 12; chip_set++)
            {
                globalstr[chip_set].c = 0;
                globalstr[chip_set].m0 = 0;
                globalstr[chip_set].saux = 0;
            }
            globalstr[chip].c = 4;
            globalstr[chip].saux = 1;
        }
        if (gmon == 2)
        { /* All off except this chip_set base M0=0 C0-C4=10100 */
            for (int chip_set = 0; chip_set < 12; chip_set++)
            {
                globalstr[chip_set].c = 0;
                globalstr[chip_set].m0 = 0;
                globalstr[chip_set].saux = 0;
            }
            globalstr[chip].c = 5;
            globalstr[chip].saux = 1;
        }
        if (gmon == 3)
        { /* All off except this chip_set thresh M0=0 C0-C4=01100*/
            for (int chip_set = 0; chip_set < 12; chip_set++)
            {
                globalstr[chip_set].c = 0;
                globalstr[chip_set].m0 = 0;
                globalstr[chip_set].saux = 0;
            }
            globalstr[chip].c = 6;
            globalstr[chip].saux = 1;
        }
        if (gmon == 4)
        { /* All off except this chip_set test pulse M0=0 C0-C4=11100*/
            for (int chip_set = 0; chip_set < 12; chip_set++)
            {
                globalstr[chip_set].c = 0;
                globalstr[chip_set].m0 = 0;
                globalstr[chip_set].saux = 0;
            }
            globalstr[chip].c = 7;
            globalstr[chip].saux = 1;
        }
        if (gmon == 5)
        { /* All off except this chip_set channel monitor M0=1; channel number to C0-C4*/
            for (int chip_set = 0; chip_set < 12; chip_set++)
            {
                globalstr[chip_set].c = 0;
                globalstr[chip_set].m0 = 0;
                globalstr[chip_set].saux = 0;
            }
            globalstr[chip].c = monch % 32;
            globalstr[chip].m0 = 1;
            globalstr[chip].saux = 1;
        }

        status = sendMarsConfiguration();
    
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumGMON, value );
    }

    else if ( function == GermaniumPUEN )
    {
        // Pileup rejection enable - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].spur = value;
        }
        status = sendMarsConfiguration();
    
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumPUEN, value );
    }

    else if ( function == GermaniumMFS )
    {
        // Multi-fire suppression - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].sse = value;
        }
        status = sendMarsConfiguration();
    
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumMFS, value );
    }

    else if ( function == GermaniumTDS )
    {
        errlogPrintf( "Set time detector slope\n" );
        int tds;
        getIntegerParam( GermaniumTDS, &tds );
        int j, rt;
        switch ( tds )
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

        for (int chip = 0; chip < 12/*nchips_*/; chip++)
        {
            globalstr[chip].tr = j; 
            globalstr[chip].rt = rt;
        }

        status = sendMarsConfiguration();
    
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumTDS, value );
    }

    else if ( function == GermaniumTDM )
    {
        // TDC mode - update globalstr for all chips and send bulk config
        for (int chip = 0; chip < 12/*nchips*/; chip++)
        {
            globalstr[chip].tm = value;
        }
        status = sendMarsConfiguration();
    
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumTDM, value );
    }

    else if ( function == GermaniumMONCH )
    {
        errlogPrintf( "Select to monitor channel %d\n", value );
        // Set CHAN field to the current chip * 64 + MONCH value
        if ( value < 0 )
            value = 0;
        else
        {
            if ( value > nelm_ - 1 )
                value = nelm_ - 1;
        }

        int chip_set = value / 32;
        int chan_set = value % 32;

//        for ( int i = 0; i < 4096; i++ )
//        {
//            spct[i]  = mca[4096 * value + i];
//            spctx[i] = (float)i * slp[value] + offs[value];
//        }
//        errlogPrintf( "SPCT and SPCTX updated\n" );

        int gmon, monch, loao;
        getIntegerParam( GermaniumGMON, &gmon );
        getIntegerParam( GermaniumMONCH, &monch );
        getIntegerParam( GermaniumLOAO, &loao );

        if (gmon == 5)
        {
            for ( int chip = 0; chip < 12; chip++ )
            {
                globalstr[chip].c = 0;
                globalstr[chip].m0 = 0;
                globalstr[chip].saux = 0;
            }
            globalstr[chip_set].c = chan_set;
            globalstr[chip_set].m0 = 1;
            globalstr[chip_set].saux = 1;
            channelstr[value].sel = loao;
            status = sendMarsConfiguration();
        }
    
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumMONCH, value );

        publishSPCT();

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

//    else if ( function == GermaniumCHIP )
//    {
//        int currentChan;
//        getIntegerParam(GermaniumCHAN, &currentChan);
//        int newChan = value * 64 + (currentChan % 64);
//        setIntegerParam(GermaniumCHAN, newChan);
//        
//        // If we're in global monitor mode 5, update the hardware
//        int gmonMode;
//        getIntegerParam(GermaniumGMON, &gmonMode);
//        if (gmonMode == 5)
//        {
//            int monch;
//            getIntegerParam(GermaniumMONCH, &monch);
//            if ( value >= 0 && value < nchips_ )
//            {
//                globalstr[value].c = monch;
//                globalstr[value].m0 = 1;
//                globalstr[value].saux = 1;
//                status = sendMarsConfiguration();
//            }
//        }
//    }
//
//    else if ( function == GermaniumCHAN )
//    {
//        // Channel selection - implement exact zDDM logic
//        // Extract chip and monitor channel from absolute channel
//        int chip = value / 64;
//        int monch = value % 64;
//        
//        // Update CHIP and MONCH fields
//        setIntegerParam(GermaniumCHIP, chip);
//        setIntegerParam(GermaniumMONCH, monch);
//        
//        // If we're in global monitor mode 5, update the hardware
//        int gmonMode;
//        getIntegerParam(GermaniumGMON, &gmonMode);
//        if (gmonMode == 5 && chip >= 0 && chip < nchips)
//        {
//            globalstr[chip].c = monch;
//            globalstr[chip].m0 = 1;
//            globalstr[chip].saux = 1;
//            status = sendMarsConfiguration();
//        }
//    }
//
//
//    else if ( function == GermaniumNELM )
//    {
//        status = udpRegisterWrite( NELM, value );
//    
//        if ( status == asynSuccess )
//            status = setIntegerParam( GermaniumNELM, value );
//    }

    //else if ( function == GermaniumCONT )
    //{
    //    state
    //}

    else if ( function == GermaniumLOAO )
    {
        errlogPrintf( "Set channel monitor to leakage/pulse\n" );
        int monch;
        getIntegerParam( GermaniumMONCH, &monch );
        channelstr[monch].sel = value;
        status = sendMarsConfiguration();
    
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumLOAO, value );
    }

    else if ( function == GermaniumTPAMP )
    {
        errlogPrintf( "Set test pulse amplitude\n" );
        for (int chip = 0; chip < 12/*nchips_*/; chip++)
        {
            globalstr[chip].pb = value;
        }

        status = udpRegisterWrite( MARS_CALPULSE, value);
    
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumTPAMP, value );
    }

    else if ( function == GermaniumCHEN_SET ) // In .bob, create a button to write 1 then 0 to CHEN_SET.PROC
    {
        int monch;

        switch( value )
        {
            case 0:
                getIntegerParam( GermaniumMONCH, &monch );
                chen_[monch] = 1;
                channelstr[monch].sm = 1;
                break;
            case 1:
                for( int i = 0; i < nelm_; i++ )
                {
                    chen_[i] = 1;
                    channelstr[i].sm = 1;
                    break;
                }
            case 2:
                getIntegerParam( GermaniumMONCH, &monch );
                chen_[monch] = 0;
                channelstr[monch].sm = 0;
                break;
            case 3:
                for( int i = 0; i < nelm_; i++ )
                {
                    chen_[i] = 0;
                    channelstr[i].sm = 0;
                    break;
                }
            default:
                errlogPrintf( "Wrong value %d for CHEN_SET\n", value );
                return asynError;
        }
        doCallbacksInt8Array( chen_, nelm_, GermaniumCHEN, 0 );
        setIntegerParam( GermaniumCHEN_SET, value );
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumTSEN_SET ) // In .bob, create a button to write 1 then 0 to TSEN_SET.PROC
    {
        int monch;

        switch( value )
        {
            case 0:
                getIntegerParam( GermaniumMONCH, &monch );
                tsen_[monch] = 1;
                channelstr[monch].st = 1;
                break;
            case 1:
                for( int i = 0; i < nelm_; i++ )
                {
                    tsen_[i] = 1;
                    channelstr[i].st = 1;
                    break;
                }
            case 2:
                getIntegerParam( GermaniumMONCH, &monch );
                tsen_[monch] = 0;
                channelstr[monch].st = 0;
                break;
            case 3:
                for( int i = 0; i < nelm_; i++ )
                {
                    tsen_[i] = 0;
                    channelstr[i].st = 0;
                    break;
                }
            default:
                errlogPrintf( "Wrong value %d for TSEN_SET\n", value );
                return asynError;
        }

        doCallbacksInt8Array( tsen_, nelm_, GermaniumTSEN, 0 );
        setIntegerParam( GermaniumTSEN_SET, value );
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumTPFRQ )
    {
        errlogPrintf( "Set test pulse frequency\n" );
        status  = udpRegisterWrite( CALPULSE_RATE, 25000000 / value / 2);
        
        if ( status == asynSuccess )
            status = udpRegisterWrite( CALPULSE_WIDTH, 25000000 / value / 2);

        if ( status == asynSuccess )
            status = sendMarsConfiguration();
    
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumTPFRQ, value );
    }

    else if ( function == GermaniumTPCNT )
    {
        status  = udpRegisterWrite( CALPULSE_CNT, value);
        if ( status == asynSuccess )
        {
            status = sendMarsConfiguration();
    
            if ( status == asynSuccess )
                status = setIntegerParam( GermaniumTPCNT, value );
        }
    }

    else if ( function == GermaniumTPENB )
    {
        if ( value == 1 )
        {
            status  = udpRegisterWrite( MARS_CALPULSE, 0xFFF );
            if ( status == asynSuccess )
                status = udpRegisterWrite( CALPULSE_MODE, 1 );
        }
        else
        {
            status  = udpRegisterWrite( MARS_CALPULSE, 0 );
            if ( status == asynSuccess )
                status = udpRegisterWrite( CALPULSE_MODE, 0 );
        }
        if ( status == asynSuccess )
        {
            status = sendMarsConfiguration();
    
            if ( status == asynSuccess )
                status = setIntegerParam( GermaniumTPENB, value );
        }

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
            errlogPrintf("Germanium: File size too small, setting to 1MB minimum\n");
            status = setIntegerParam(GermaniumFSIZE, 1);
        }
        else if (value > 1000) // Maximum 1TB
        {
            errlogPrintf("Germanium: File size too large, setting to 1TB maximum\n");
            status = setIntegerParam(GermaniumFSIZE, 1000);
        }
        else
        {
            if ( status == asynSuccess )
            {
                status = setIntegerParam( GermaniumFSIZE, value );
                errlogPrintf("Germanium: Maximum file size set to %d MBytes\n", value);
            }
        }
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
        
        status = setIntegerParam( GermaniumCNT, value );
    }

    else if ( function == GermaniumTP )
    {
        // Time preset - use COUNT_TIME registers
        uint64_t ticks = (uint64_t)(value * 1000000);
        status  = udpRegisterWrite( COUNT_TIME_LO, ticks & 0xFFFF );
        if ( status == asynSuccess )
        {
            status = udpRegisterWrite( COUNT_TIME_HI, (ticks >> 16) & 0xFFFF );
            if ( status == asynSuccess )
                status = setIntegerParam( function, value );
        }
    }

    else if ( function == GermaniumRUNNO )
    {
        status = udpRegisterWrite( FRAME_NO, value );
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumRUNNO, value );
    }
    
    else if ( function == GermaniumMODE )
    {
        // Stop counting if current mode==1
        // CNT = 0;
        // frame_done(1);

        status = udpRegisterWrite( COUNT_MODE, value);
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumMODE, value );
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
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumPLDEL, value );
    }

    else if ( function == GermaniumRODEL )
    {
        errlogPrintf( "Set readout delay to %d\n", value );

        status = udpRegisterWrite( MARS_RDOUT_ENB, value);
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumRODEL, value );
    }

    else if ( function == GermaniumADC0_SKEW )
    {
        errlogPrintf( "Set ADC0 skew to %d\n", value );

        status = ad9252_cnfg( 0, value);
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumADC0_SKEW, value );
    }
    else if ( function == GermaniumADC1_SKEW )
    {
        errlogPrintf( "Set ADC1 skew to %d\n", value );

        status = ad9252_cnfg( 1, value);
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumADC1_SKEW, value );
    }

    else if ( function == GermaniumADC2_SKEW )
    {
        errlogPrintf( "Set ADC2 skew to %d\n", value );

        status = ad9252_cnfg( 2, value);
        if ( status == asynSuccess )
            status = setIntegerParam( GermaniumADC2_SKEW, value );
    }

    // Add more parameter mappings as needed
    else
    {
        errlogPrintf( "Invalid function %d for writeInt32()\n", function );
        status = asynError;
    }
        
    if (status == asynSuccess)
    {
        callParamCallbacks(0);
    }
    else
    {
        errlogPrintf("Germanium: Failed to send parameter %d to device\n", function);
    }

    return status;
}

//===========================================================================//

asynStatus germaniumDetector::writeFloat64( asynUser *pasynUser, epicsFloat64 value )
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    errlogPrintf( "[%s]: function is %d\n", __func__, function );

    // First set the parameter locally
    status = ADDriver::setDoubleParam(function, value);

    if (status != asynSuccess)
    {
        errlogPrintf( "setDoubleParam failed with status %d\n", status );
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
            errlogPrintf("Germanium: RATE bounded to %f Hz\n", rate);
        }
        // RATE is primarily read-only display parameter, don't send to hardware
    }

    else if ( function == GermaniumHV )
    {
        uint32_t hv = static_cast<uint32_t>(8.19f * value);
        status = udpRegisterWrite( HV, hv );
        if (status == asynSuccess)
        {
            status = setDoubleParam( GermaniumHV, value );
=======
    else if ( function == HV ):
    {
        hv = static_cast<int>(value)/10;

        status = setDoubleParam( GermaniumHV, hv );
        if ( status == asynSuccess )
        {
            udpRegisterWrite( HV, hv );
>>>>>>> 6bd94642f8b1b29366707f81c07ae1085584a3f9
        }
    }

    else
    {
        errlogPrintf( "Invalid function %d for writeFloat64()\n", function );
        status = asynError;
    }

    if (status == asynSuccess)
    {
        callParamCallbacks(0);
    }
    else
    {
        errlogPrintf("Germanium: Failed to send float parameter %d to device\n", function);
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
        errlogPrintf( "setDoubleParam failed with status %d\n", status );
        return status;
    }
    
    // Send string parameters to device if needed
    if ( function == GermaniumFNAM )
    {
        errlogPrintf( "Set data file name %s\n", value );
        setStringParam( function, value );
    }

    else if ( function == GermaniumCALF )
    {
        errlogPrintf( "Set calibration file name %s\n", value );
        setStringParam( function, value );

    }
    else if ( function == GermaniumDIR )
    {
        errlogPrintf( "Set data file directory %s\n", value );
        // Data directory - create if it doesn't exist
        createDataDirectory();
    }
    
    else if ( function == GermaniumIPADDR )
    {
        errlogPrintf( "Set UDP IP address to %s\n", value);

        uint32_t host;
        if (!ipStrToU32Host(value, host))
        {
            errlogPrintf( "Invalid IP '%s'\n", value );
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
        errlogPrintf( "Invalid function %d for writeFloat64()\n", function );
        status = asynError;
    }

    if (status == asynSuccess)
    {
        callParamCallbacks(0);
    }
    else
    {
        errlogPrintf("Germanium: Failed to send string parameter %d to device\n", function);
    }

    *nActual = strlen(value);

    return status;
}

//===========================================================================//

asynStatus germaniumDetector::readInt32( asynUser *pasynUser, int* value )
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
        errlogPrintf( "Invalid function %d for readInt32()\n", function );
        status = asynError;
    }

    if (status == asynSuccess)
    {
        callParamCallbacks(0);
    }
    else
    {
        errlogPrintf("Germanium: Failed to read int32 parameter %d from device\n", function);
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
        errlogPrintf( "Invalid function %d for readInt32Array()\n", function );
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
        //uint8_t *chenArray = new uint8_t[nElements];
        for ( int chan = 0; chan < nelm_; chan++ )
        {
            chen_[chan] = value[chan];
            channelstr[chan].sm = value[chan];
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumPUTR )
    {
        for ( int chan = 0; chan < nelm_; chan++ )
        {
            channelstr[chan].dp = value[chan];
            status = sendMarsConfiguration();
        }
    }

    else if ( function == GermaniumTHRSH )
    {
        for ( int chip = 0; chip < 12/*nchips_*/; chip++ )
        {
            thrsh_[chip] = value[chip];
            globalstr[chip].pa = value[chip];
        }
        status = sendMarsConfiguration();
    }

    else if ( function == GermaniumTSEN )
    {
        for ( int chan = 0; chan < nelm_; chan++ )
        {
            tsen_[chan] = value[chan];
            channelstr[chan].st = value[chan];
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
        errlogPrintf("Germanium: Unknown array parameter %d in writeInt32Array\n", function);
    }

    if (status == asynSuccess)
    {
        callParamCallbacks(0);
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
        errlogPrintf("Received non-response message on control channel\n");
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
            char* verString = ntohl(((UdpRxMsg*)receiveBuffer)->payload.single_word.data);
            setStringParam( GermaniumFVER, verString );
            break;
        //----------------------------------------------//
        case UDP_IP_ADDR:
            uint32_t ip = ntohl(((UdpRxMsg*)receiveBuffer)->payload.single_word.data);
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                     (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                     (ip >> 8) & 0xFF, ip & 0xFF);
            setStringParam( germaniumIPAddrRbvString, ipStr );
            break;
        //----------------------------------------------//
        case TRIG:
            uint32_t trig = ntohl(((UdpRxMsg*)receiveBuffer)->payload.single_word.data);
            setIntegerParam( GermaniumCNT, trig );
            break;
        //----------------------------------------------//
        case FRAME_NO:
            uint32_t frameNo = ntohl(((UdpRxMsg*)receiveBuffer)->payload.single_word.data);
            setIntegerParam( GermaniumRUNNO, frameNo );
            break;
        //----------------------------------------------//
        case TEMP1:
            int temp_raw = ntohl(((UdpRxMsg*)receiveBuffer)->payload.single_word.data);
            temp_raw >>= 4;
            double temp = tmp_raw * 0.0625f;

            setDoubleParam( GermaniumTEMP1, temp );
            break;
        //----------------------------------------------//
        case TEMP2:
            int temp_raw = ntohl(((UdpRxMsg*)receiveBuffer)->payload.single_word.data);
            temp_raw >>= 4;
            double temp = temp_raw * 0.0625f;

            setDoubleParam( GermaniumTEMP2, temp );
            break;
        //----------------------------------------------//
        case TEMP3:
            int temp_raw = ntohl(((UdpRxMsg*)receiveBuffer)->payload.single_word.data);
            temp_raw >>= 4;
            double temp = temp_raw * 0.0625f;

            setDoubleParam( GermaniumTEMP3, temp );
            break;
        //----------------------------------------------//
        case ZTEMP:
            int temp_raw = ntohl(((UdpRxMsg*)receiveBuffer)->payload.single_word.data);
            double temp = 503.975 * temp_raw / 4096 - 273.15;

            setDoubleParam( GermaniumZTEMP, temp );
            break;
        //----------------------------------------------//
        case HV_RBV:
        {
            double hv_rbv = static_cast<double>(val) * 0.122100122f;
            setDoubleParam( GermaniumHV_RBV, hv_rbv);
            break;
        }
        //----------------------------------------------//
        case HV_CURR:
            double hv_curr = static_cast<double>(ntohl(((UdpRxMsg*)receiveBuffer)->payload.single_word.data)) * 0.001220703;
            setDoubleParam( GermaniumHV_CURR, hv_curr );
            break;
        //----------------------------------------------//
        default:
            errlogPrintf( "Value received for unknown register %d\n", reg );
        //----------------------------------------------//
    }

    callParamCallbacks(0);

}

//===========================================================================//

void germaniumDetector::report(FILE *fp, int details)
{
    fprintf(fp, "Germanium detector: %d elements, %d chips\n", nelm_, nchips_);
    fprintf(fp, "UDP IP Address: %s\n", ipAddress);

    if (details > 1)
    {
//        errlogPrintf(fp, "Device FD: %d\n", device_fd);
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

