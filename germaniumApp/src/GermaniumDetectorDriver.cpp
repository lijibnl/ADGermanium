/**
 * @file GermaniumDetectorDriver.cpp
 * @brief asynPortDriver interface implementations for GermaniumDetector (async ZMQ version).
 *
 * All operations use zmqTx() for fire-and-forget writes.
 * Reads return cached values; the Control Rx thread updates the cache.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "GermaniumDetector.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <arpa/inet.h>

//===========================================================================//

asynStatus GermaniumDetector::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    status = ADDriver::setIntegerParam(function, value);
    if (status != asynSuccess) return status;

    //------------------------------------------------------------------
    // MARS ASIC configuration parameters.
    // Delta-config: send field update → Zynq stores it → trigger load.
    // Cost: 2 ZMQ round-trips per parameter change.
    //------------------------------------------------------------------

    uint32_t allChipMask = (1U << nchips) - 1;

    if (function == GermaniumSHPT)
    {
        status = zmqMarsSetGlobal(allChipMask, MARS_FIELD_ST, value);
        if (status == asynSuccess) status = zmqMarsLoad(allChipMask);
    }
    else if (function == GermaniumGAIN)
    {
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s: GAIN: value=0x%08X\n", portName, value);
        status = zmqMarsSetGlobal(allChipMask, MARS_FIELD_GAIN, value);
        if (status == asynSuccess) status = zmqMarsLoad(allChipMask);
    }
    else if (function == GermaniumPOL)
    {
        status = zmqMarsSetGlobal(allChipMask, MARS_FIELD_POL, value);
        if (status == asynSuccess) status = zmqMarsLoad(allChipMask);
    }
    else if (function == GermaniumEBLK)
    {
        status = zmqMarsSetGlobal(allChipMask, MARS_FIELD_EBLK, value);
        if (status == asynSuccess) status = zmqMarsLoad(allChipMask);
    }
    else if (function == GermaniumPUEN)
    {
        status = zmqMarsSetGlobal(allChipMask, MARS_FIELD_PUEN, value);
        if (status == asynSuccess) status = zmqMarsLoad(allChipMask);
    }
    else if (function == GermaniumMFS)
    {
        status = zmqMarsSetGlobal(allChipMask, MARS_FIELD_MFS, value);
        if (status == asynSuccess) status = zmqMarsLoad(allChipMask);
    }
    else if (function == GermaniumTDM)
    {
        status = zmqMarsSetGlobal(allChipMask, MARS_FIELD_TDM, value);
        if (status == asynSuccess) status = zmqMarsLoad(allChipMask);
    }

    //------------------------------------------------------------------
    // Direct register writes
    //------------------------------------------------------------------
    else if (function == GermaniumTPAMP)
    {
        status = zmqTx(ZMQ_CMD_REG_WRITE, MARS_CALPULSE, value);
    }
    else if (function == GermaniumTPFRQ)
    {
        status = zmqTx(ZMQ_CMD_REG_WRITE, CALPULSE_RATE, value);
    }
    else if (function == GermaniumTPCNT)
    {
        status = zmqTx(ZMQ_CMD_REG_WRITE, CALPULSE_CNT, value);
    }
    else if (function == GermaniumTPENB)
    {
        status = zmqTx(ZMQ_CMD_REG_WRITE, CALPULSE_MODE, value);
    }
    else if (function == GermaniumPLDEL)
    {
        status = zmqTx(ZMQ_CMD_REG_WRITE, MARS_PIPE_DELAY, value);
    }
    else if (function == GermaniumRODEL)
    {
        status = zmqTx(ZMQ_CMD_REG_WRITE, MARS_RDOUT_ENB, value);
    }
    else if (function == GermaniumLOAO)
    {
        status = zmqTx(ZMQ_CMD_REG_WRITE, SIM_EVT_SEL, value);
    }
    else if (function == GermaniumMODE)
    {
        int modeReg = value ? 1 : 0;
        status = zmqTx(ZMQ_CMD_REG_WRITE, COUNT_MODE, modeReg);
        if (status == asynSuccess)
            setIntegerParam(GermaniumMODE, modeReg);
    }
    else if (function == GermaniumTDS)
    {
        status = zmqMarsSetGlobal(allChipMask, MARS_FIELD_TDS, value);
        if (status == asynSuccess) status = zmqMarsLoad(allChipMask);
    }
    else if (function == GermaniumADC0_CLK_SKEW)
    {
        status = zmqTx(ZMQ_CMD_ADC_CLK_SKEW_SET, 1, static_cast<uint32_t>(value));
    }
    else if (function == GermaniumADC1_CLK_SKEW)
    {
        status = zmqTx(ZMQ_CMD_ADC_CLK_SKEW_SET, 2, static_cast<uint32_t>(value));
    }
    else if (function == GermaniumADC2_CLK_SKEW)
    {
        status = zmqTx(ZMQ_CMD_ADC_CLK_SKEW_SET, 3, static_cast<uint32_t>(value));
    }
    else if (function == GermaniumLOG_LEVEL)
    {
        status = zmqTx(ZMQ_CMD_SET_LOG_LEVEL, 0, static_cast<uint32_t>(value));
    }

    //------------------------------------------------------------------
    // Chip/Channel selection
    //------------------------------------------------------------------
    else if (function == GermaniumCHIP)
    {
        int currentChan;
        getIntegerParam(GermaniumCHAN, &currentChan);
        int newChan = value * 32 + (currentChan % 32);
        setIntegerParam(GermaniumCHAN, newChan);

        int gmonMode;
        getIntegerParam(GermaniumGMON, &gmonMode);
        if (gmonMode == 5 && value >= 0 && value < nchips)
        {
            int monch;
            getIntegerParam(GermaniumMONCH, &monch);
            uint32_t chipBit = 1U << value;
            zmqMarsSetGlobal(chipBit, MARS_FIELD_C, monch);
            zmqMarsSetGlobal(chipBit, MARS_FIELD_M0, 1);
            zmqMarsSetGlobal(chipBit, MARS_FIELD_SAUX, 1);
            status = zmqMarsLoad(chipBit);
        }
    }
    else if (function == GermaniumCHAN)
    {
        int chip = value / 32;
        int monch = value % 32;
        setIntegerParam(GermaniumCHIP, chip);
        setIntegerParam(GermaniumMONCH, monch);

        int gmonMode;
        getIntegerParam(GermaniumGMON, &gmonMode);
        if (gmonMode == 5 && chip >= 0 && chip < nchips)
        {
            uint32_t chipBit = 1U << chip;
            zmqMarsSetGlobal(chipBit, MARS_FIELD_C, monch);
            zmqMarsSetGlobal(chipBit, MARS_FIELD_M0, 1);
            zmqMarsSetGlobal(chipBit, MARS_FIELD_SAUX, 1);
            status = zmqMarsLoad(chipBit);
        }
    }

    //------------------------------------------------------------------
    // Acquisition control
    //------------------------------------------------------------------
    else if (function == GermaniumCNT)
    {
        if (value == 1)
            startDataAcquisition();
        else
            stopDataAcquisition();
    }

    //------------------------------------------------------------------
    // Clear operations
    //------------------------------------------------------------------
    else if (function == GermaniumCLRE || function == GermaniumCLRM)
    {
        clearSpectra();
        setIntegerParam(function, 0);
    }

    //------------------------------------------------------------------
    // File size limit (in MB)
    //------------------------------------------------------------------
    else if (function == GermaniumFWEN)
    {
        fileWriteEnable.store(value);
        setIntegerParam(GermaniumFWEN, value);
    }
    else if (function == GermaniumFSIZE)
    {
        if (value < 1) value = 1;
        if (value > 1000000) value = 1000000;
        setIntegerParam(GermaniumFSIZE, value);
    }

    //------------------------------------------------------------------
    // Monitor channel and chip selection
    //------------------------------------------------------------------
    else if (function == GermaniumMONCH)
    {
        int gmonMode;
        getIntegerParam(GermaniumGMON, &gmonMode);
        if (gmonMode == 5)
        {
            int currentChip;
            getIntegerParam(GermaniumCHIP, &currentChip);
            if (currentChip >= 0 && currentChip < nchips)
            {
                uint32_t chipBit = 1U << currentChip;
                zmqMarsSetGlobal(chipBit, MARS_FIELD_C, value);
                zmqMarsSetGlobal(chipBit, MARS_FIELD_M0, 1);
                zmqMarsSetGlobal(chipBit, MARS_FIELD_SAUX, 1);
                status = zmqMarsLoad(chipBit);
            }
        }
    }
    else if (function == GermaniumGMON)
    {
        // Reset all chips
        zmqMarsSetGlobal(allChipMask, MARS_FIELD_C, 0);
        zmqMarsSetGlobal(allChipMask, MARS_FIELD_M0, 0);
        zmqMarsSetGlobal(allChipMask, MARS_FIELD_SAUX, 0);

        int currentChip;
        getIntegerParam(GermaniumCHIP, &currentChip);
        if (currentChip >= 0 && currentChip < nchips)
        {
            uint32_t chipBit = 1U << currentChip;
            switch (value)
            {
                case 1: // Temperature
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_C, 4);
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_SAUX, 1);
                    break;
                case 2: // Baseline
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_C, 5);
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_SAUX, 1);
                    break;
                case 3: // Threshold
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_C, 6);
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_SAUX, 1);
                    break;
                case 4: // Test pulse
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_C, 7);
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_SAUX, 1);
                    break;
                case 5: // Channel monitor
                {
                    int monch;
                    getIntegerParam(GermaniumMONCH, &monch);
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_C, monch);
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_M0, 1);
                    zmqMarsSetGlobal(chipBit, MARS_FIELD_SAUX, 1);
                    break;
                }
            }
        }
        status = zmqMarsLoad(allChipMask);
    }

    //------------------------------------------------------------------
    // Simplified channel enable / test-enable operations
    //------------------------------------------------------------------
    else if (function == GermaniumCHEN_SEL)
    {
        int chan;
        getIntegerParam(GermaniumCHAN, &chan);
        if (chan >= 0 && chan < numElements)
        {
            zmqMarsSetChannel(chan, MARS_CH_CHEN, value ? 1 : 0);
            status = zmqMarsLoad((1U << nchips) - 1);
        }
    }
    else if (function == GermaniumCHEN_ALL)
    {
        zmqMarsSetChannel(0xFFF, MARS_CH_CHEN, value ? 1 : 0);
        status = zmqMarsLoad((1U << nchips) - 1);
    }
    else if (function == GermaniumTSEN_SEL)
    {
        int chan;
        getIntegerParam(GermaniumCHAN, &chan);
        if (chan >= 0 && chan < numElements)
        {
            zmqMarsSetChannel(chan, MARS_CH_TSEN, value ? 1 : 0);
            status = zmqMarsLoad((1U << nchips) - 1);
        }
    }
    else if (function == GermaniumTSEN_ALL)
    {
        zmqMarsSetChannel(0xFFF, MARS_CH_TSEN, value ? 1 : 0);
        status = zmqMarsLoad((1U << nchips) - 1);
    }

    else
    {
        // Parameters that are stored locally only (no hardware action)
        // e.g. display settings, run number, etc.
        status = asynSuccess;
    }

    if (status == asynSuccess)
        callParamCallbacks();

    return status;
}

//===========================================================================//

asynStatus GermaniumDetector::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    //------------------------------------------------------------------
    // Async model: return cached value, queue a read request.
    //------------------------------------------------------------------

    if (function == GermaniumFVER)
    {
        //zmqTx(ZMQ_CMD_REG_READ, VERSIONREG, 0);
        getIntegerParam(GermaniumFVER, value);
    }
    else if (function == GermaniumDETMODEL)
    {
        //zmqTx(ZMQ_CMD_REG_READ, DETECTOR_MODEL, 0);
        getIntegerParam(GermaniumDETMODEL, value);
    }
    else if (function == GermaniumTPAMP_RBV)
    {
        //zmqTx(ZMQ_CMD_REG_READ, MARS_CALPULSE, 0);
        getIntegerParam(GermaniumTPAMP_RBV, value);
    }
    else if (function == GermaniumTPFRQ_RBV)
    {
        //zmqTx(ZMQ_CMD_REG_READ, CALPULSE_RATE, 0);
        getIntegerParam(GermaniumTPFRQ_RBV, value);
    }
    else if (function == GermaniumTPCNT_RBV)
    {
        //zmqTx(ZMQ_CMD_REG_READ, CALPULSE_CNT, 0);
        getIntegerParam(GermaniumTPCNT_RBV, value);
    }
    else if (function == GermaniumTPENB_RBV)
    {
        //zmqTx(ZMQ_CMD_REG_READ, CALPULSE_MODE, 0);
        getIntegerParam(GermaniumTPENB_RBV, value);
    }
    else if (function == GermaniumPLDEL_RBV)
    {
        //zmqTx(ZMQ_CMD_REG_READ, MARS_PIPE_DELAY, 0);
        getIntegerParam(GermaniumPLDEL_RBV, value);
    }
    else if (function == GermaniumRODEL_RBV)
    {
        //zmqTx(ZMQ_CMD_REG_READ, MARS_RDOUT_ENB, 0);
        getIntegerParam(GermaniumRODEL_RBV, value);
    }
    else if (function == GermaniumMODE)
    {
        //zmqTx(ZMQ_CMD_REG_READ, COUNT_MODE, 0);
        getIntegerParam(GermaniumMODE, value);
    }
    else if (function == GermaniumCNT_RBV)
    {
        //zmqTx(ZMQ_CMD_REG_READ, TRIG, 0);
        getIntegerParam(GermaniumCNT_RBV, value);
    }
    else
    {
        return ADDriver::readInt32(pasynUser, value);
    }

    return status;
}

//===========================================================================//

asynStatus GermaniumDetector::writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                      size_t nElements)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    if (function == GermaniumTHRSH)
    {
        size_t count = std::min(nElements, static_cast<size_t>(nchips));
        for (size_t i = 0; i < count; i++)
            zmqMarsSetGlobal(1U << i, MARS_FIELD_TH, value[i]);
        status = zmqMarsLoad((1U << nchips) - 1);
    }
    else
    {
        status = asynSuccess;
    }

    if (status == asynSuccess)
        callParamCallbacks();
    return status;
}

//===========================================================================//

asynStatus GermaniumDetector::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                     size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;

    if (function == GermaniumMCA)
    {
        size_t totalSize = static_cast<size_t>(numElements) * SPECTRUM_SIZE;
        size_t copySize = std::min(nElements, totalSize);
        for (size_t i = 0; i < copySize; i++)
            value[i] = mcaData[i].load(std::memory_order_relaxed);
        *nIn = copySize;
    }
    else if (function == GermaniumTDC)
    {
        size_t totalSize = static_cast<size_t>(numElements) * TDC_SIZE;
        size_t copySize = std::min(nElements, totalSize);
        for (size_t i = 0; i < copySize; i++)
            value[i] = tdcData[i].load(std::memory_order_relaxed);
        *nIn = copySize;
    }
    else if (function == GermaniumSPCT)
    {
        int monch;
        getIntegerParam(GermaniumMONCH, &monch);
        int chip;
        getIntegerParam(GermaniumCHIP, &chip);
        int element = chip * 32 + monch;
        if (element < 0 || element >= numElements) element = 0;

        size_t copySize = std::min(nElements, static_cast<size_t>(SPECTRUM_SIZE));
        size_t base = static_cast<size_t>(element) * SPECTRUM_SIZE;
        for (size_t i = 0; i < copySize; i++)
            value[i] = mcaData[base + i].load(std::memory_order_relaxed);
        *nIn = copySize;
    }
    else if (function == GermaniumINTENS)
    {
        size_t copySize = std::min(nElements, static_cast<size_t>(numElements));
        for (size_t i = 0; i < copySize; i++)
            value[i] = countRates[i].load(std::memory_order_relaxed);
        *nIn = copySize;
    }
    else
    {
        memset(value, 0, nElements * sizeof(epicsInt32));
        *nIn = nElements;
    }

    return asynSuccess;
}

//===========================================================================//

asynStatus GermaniumDetector::writeOctet(asynUser *pasynUser, const char *value,
                                 size_t maxChars, size_t *nActual)
{
    std::cout << "[" << __func__ << "]: writeOctet: function=" << pasynUser->reason << ", value='" << value << "'\n";
    asynPrint( pasynUserSelf
             , ASYN_TRACEIO_DRIVER
             , "[%s]: writeOctet: function=%d, value='%s'\n"
             , portName
             , pasynUser->reason
             , value
             );

    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    status = ADDriver::setStringParam(function, value);
    if (status != asynSuccess) return status;

    if (function == GermaniumDIR)
    {
        createDataDirectory();
    }
    else if (function == GermaniumIPADDR)
    {
        // Validate IP address format
        struct in_addr addr;
        
        asynPrint( pasynUserSelf
                 , ASYN_TRACEIO_DRIVER
                 , "[%s]: Setting IP address to '%s'\n"
                 , portName
                 , value
                 );
        if (inet_pton(AF_INET, value, &addr) != 1)
        {
            asynPrint( pasynUserSelf
                     , ASYN_TRACEIO_DRIVER
                     , "[%s]: invalid IP address '%s'\n"
                     , portName
                     , value
                     );
            return asynError;
        }
        // Write to FPGA register for PL UDP destination
        // inet_pton produces network byte order; FPGA expects host byte order
        status = zmqTx(ZMQ_CMD_REG_WRITE, UDP_IP_ADDR, ntohl(addr.s_addr));
    }

    if (status == asynSuccess)
        callParamCallbacks();

    *nActual = strlen(value);
    return status;
}

//===========================================================================//

asynStatus GermaniumDetector::readOctet(asynUser *pasynUser, char *value,
                                        size_t maxChars, size_t *nActual, int *eomReason)
{
    if (pasynUser->reason == GermaniumIPADDR_RBV)
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACEIO_DRIVER
                 , "[%s]: Reading IP address from FPGA register\n"
                 , portName
                 );
        //zmqTx(ZMQ_CMD_REG_READ, UDP_IP_ADDR, 0);
        getStringParam(GermaniumIPADDR_RBV, 15, value);
        *nActual = strlen(value);
        *eomReason = ASYN_EOM_END;
    }

    return asynPortDriver::readOctet(pasynUser, value, maxChars, nActual, eomReason);
}

//===========================================================================//

asynStatus GermaniumDetector::writeInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                     size_t nElements)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    size_t count = std::min(nElements, static_cast<size_t>(numElements));

    //if (function == GermaniumCHEN)
    //{
    //    for (size_t i = 0; i < count; i++)
    //        zmqMarsSetChannel(i, MARS_CH_CHEN, (value[i] != 0) ? 1 : 0);
    //    status = zmqMarsLoad((1U << nchips) - 1);
    //}
    //else if (function == GermaniumTSEN)
    //{
    //    for (size_t i = 0; i < count; i++)
    //        zmqMarsSetChannel(i, MARS_CH_TSEN, (value[i] != 0) ? 1 : 0);
    //    status = zmqMarsLoad((1U << nchips) - 1);
    //}
    //else
    if (function == GermaniumTHTR)
    {
        for (size_t i = 0; i < count; i++)
            zmqMarsSetChannel(i, MARS_CH_THTR, value[i]);
        status = zmqMarsLoad((1U << nchips) - 1);
    }
    else if (function == GermaniumPUTR)
    {
        for (size_t i = 0; i < count; i++)
            zmqMarsSetChannel(i, MARS_CH_PUTR, value[i]);
        status = zmqMarsLoad((1U << nchips) - 1);
    }

    if (status == asynSuccess)
        callParamCallbacks();
    return status;
}

//===========================================================================//

asynStatus GermaniumDetector::readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                    size_t nElements, size_t *nIn)
{
    // Zynq is authoritative for MARS config — IOC does not cache.
    // Return the last-written values from the asyn parameter library.
    // Per-channel arrays are write-only from the IOC side.
    memset(value, 0, nElements);
    *nIn = nElements;
    return asynSuccess;
}

//===========================================================================//

asynStatus GermaniumDetector::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    status = ADDriver::setDoubleParam(function, value);
    if (status != asynSuccess) return status;

    if (function == GermaniumTP)
    {
        // Time preset in seconds → FPGA COUNT_TIME registers (25 MHz clock)
        uint64_t ticks = static_cast<uint64_t>(value * 25.0e6);
        zmqTx(ZMQ_CMD_REG_WRITE, COUNT_TIME_LO, static_cast<uint32_t>(ticks & 0xFFFFFFFF));
        status = zmqTx(ZMQ_CMD_REG_WRITE, COUNT_TIME_HI, static_cast<uint32_t>(ticks >> 32));
    }
    else if (function == GermaniumDLY)
    {
        uint32_t delayReg = static_cast<uint32_t>(value * 1000);
        status = zmqTx(ZMQ_CMD_REG_WRITE, TD_CAL, delayReg);
    }
    else if (function == GermaniumHV)
    {
        // DAC7678 channel 5 for HV, scale: 8.19 counts/V
        uint32_t dacCode = static_cast<uint32_t>(8.19 * value);
        status = zmqTx(ZMQ_CMD_I2C_DAC_WRITE, DAC_CH_HV, dacCode);
    }
    else if (function == GermaniumP1)
    {
        // DAC7678 channel 6 for Peltier 1, scale: 819 counts/V
        uint32_t dacCode = static_cast<uint32_t>(819.0 * value);
        status = zmqTx(ZMQ_CMD_I2C_DAC_WRITE, DAC_CH_P1, dacCode);
    }
    else if (function == GermaniumP2)
    {
        // DAC7678 channel 2 for Peltier 2, scale: 819 counts/V
        uint32_t dacCode = static_cast<uint32_t>(819.0 * value);
        status = zmqTx(ZMQ_CMD_I2C_DAC_WRITE, DAC_CH_P2, dacCode);
    }
    else
    {
        // Locally stored parameters (FREQ, RATE, RAT1, etc.)
        status = asynSuccess;
    }

    if (status == asynSuccess)
        callParamCallbacks();

    return status;
}

//===========================================================================//

asynStatus GermaniumDetector::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    //------------------------------------------------------------------
    // Async model: return cached value, queue a read request.
    // The Control Rx thread updates the cache when the reply arrives.
    // EPICS SCAN drives the polling rate.
    //------------------------------------------------------------------

    if (function == GermaniumTP)
    {
        getDoubleParam(GermaniumTP, value);
    }
    else if (function == GermaniumT)
    {
        getDoubleParam(GermaniumT, value);
    }
    else if (function == GermaniumTEMP1)
    {
        getDoubleParam(GermaniumTEMP1, value);
    }
    else if (function == GermaniumTEMP2)
    {
        getDoubleParam(GermaniumTEMP2, value);
    }
    else if (function == GermaniumTEMP3)
    {
        getDoubleParam(GermaniumTEMP3, value);
    }
    else if (function == GermaniumZTEMP)
    {
        getDoubleParam(GermaniumZTEMP, value);
    }
    else if (function == GermaniumHV_RBV)
    {
        getDoubleParam(GermaniumHV_RBV, value);
    }
    else if (function == GermaniumHV_CURR)
    {
        getDoubleParam(GermaniumHV_CURR, value);
    }
    else if (function == GermaniumP1_CURR)
    {
        getDoubleParam(GermaniumP1_CURR, value);
    }
    else if (function == GermaniumP2_CURR)
    {
        getDoubleParam(GermaniumP2_CURR, value);
    }
    else
    {
        return ADDriver::readFloat64(pasynUser, value);
    }

    return status;
}

//===========================================================================//

asynStatus GermaniumDetector::readFloat64Array(asynUser *pasynUser, epicsFloat64 *value, size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    if (function == GermaniumSPCTX) {
        // Example: fill with dummy calibration data. Replace with real logic.
        size_t copySize = std::min(nElements, static_cast<size_t>(SPECTRUM_SIZE));
        for (size_t i = 0; i < copySize; ++i)
            value[i] = static_cast<double>(i) * 0.1; // Example calibration
        *nIn = copySize;
        return asynSuccess;
    }
    // Default: zero fill
    memset(value, 0, nElements * sizeof(epicsFloat64));
    *nIn = nElements;
    return asynSuccess;
}

//===========================================================================//

void GermaniumDetector::report(FILE *fp, int details)
{
    fprintf( fp
           , "Germanium ZMQ detector: %d elements, %d chips\n"
           , numElements
           , nchips
           );
    fprintf( fp
           , "ZMQ target: %s (cmd port %s, reply port %s)\n"
           , ipAddress
           , ZMQ_CMD_PORT
           , ZMQ_REPLY_PORT
           );
    fprintf( fp
           , "Acquisition: %s\n"
           , acquisitionRunning ? "Running" : "Idle"
           );

    ADDriver::report(fp, details);
}

//===========================================================================//

asynStatus GermaniumDetector::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                    const char **pptypeName, size_t *psize)
{
    return ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
}

//===========================================================================//

