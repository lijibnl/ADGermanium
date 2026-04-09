/**
 * @file germaniumDetectorDriver.cpp
 * @brief asynPortDriver interface implementations for germaniumDetector (ZMQ version).
 *
 * All register operations use zmqRegisterWrite/Read instead of UDP.
 * MARS configuration operations that require non-register access (I2C, SPI)
 * are not supported by the current C ZMQ server and are noted as such.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "germaniumDetector.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <arpa/inet.h>

//===========================================================================//

asynStatus germaniumDetector::writeInt32(asynUser *pasynUser, epicsInt32 value)
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
        printf("[%s] - GAIN: value=0x%08X\n",
               __func__, value);
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
        status = zmqRegisterWrite(MARS_CALPULSE, value);
    }
    else if (function == GermaniumTPFRQ)
    {
        status = zmqRegisterWrite(CALPULSE_RATE, value);
    }
    else if (function == GermaniumTPCNT)
    {
        status = zmqRegisterWrite(CALPULSE_CNT, value);
    }
    else if (function == GermaniumTPENB)
    {
        status = zmqRegisterWrite(CALPULSE_MODE, value);
    }
    else if (function == GermaniumPLDEL)
    {
        status = zmqRegisterWrite(MARS_PIPE_DELAY, value);
    }
    else if (function == GermaniumRODEL)
    {
        status = zmqRegisterWrite(MARS_RDOUT_ENB, value);
    }
    else if (function == GermaniumLOAO)
    {
        status = zmqRegisterWrite(SIM_EVT_SEL, value);
    }
    else if (function == GermaniumMODE)
    {
        int modeReg = value ? 1 : 0;
        status = zmqRegisterWrite(COUNT_MODE, modeReg);
        if (status == asynSuccess)
            setIntegerParam(GermaniumMODE, modeReg);
    }
    else if (function == GermaniumTDS)
    {
        status = zmqMarsSetGlobal(allChipMask, MARS_FIELD_TDS, value);
        if (status == asynSuccess) status = zmqMarsLoad(allChipMask);
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

asynStatus germaniumDetector::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    status = ADDriver::setDoubleParam(function, value);
    if (status != asynSuccess) return status;

    if (function == GermaniumTP)
    {
        // Time preset in seconds → FPGA COUNT_TIME registers (25 MHz clock)
        uint64_t ticks = static_cast<uint64_t>(value * 25.0e6);
        status = zmqRegisterWrite(COUNT_TIME_LO, static_cast<uint32_t>(ticks & 0xFFFFFFFF));
        if (status == asynSuccess)
            status = zmqRegisterWrite(COUNT_TIME_HI, static_cast<uint32_t>(ticks >> 32));
    }
    else if (function == GermaniumDLY)
    {
        uint32_t delayReg = static_cast<uint32_t>(value * 1000);
        status = zmqRegisterWrite(TD_CAL, delayReg);
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

asynStatus germaniumDetector::writeOctet(asynUser *pasynUser, const char *value,
                                 size_t maxChars, size_t *nActual)
{
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
        if (inet_pton(AF_INET, value, &addr) != 1)
        {
            printf("Germanium: Invalid IP address '%s'\n", value);
            return asynError;
        }
        // Write to FPGA register for PL UDP destination
        // inet_pton produces network byte order; FPGA expects host byte order
        status = zmqRegisterWrite(UDP_IP_ADDR, ntohl(addr.s_addr));
    }

    if (status == asynSuccess)
        callParamCallbacks();

    *nActual = strlen(value);
    return status;
}

//===========================================================================//

asynStatus germaniumDetector::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
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

asynStatus germaniumDetector::writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
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

void germaniumDetector::report(FILE *fp, int details)
{
    fprintf(fp, "Germanium ZMQ detector: %d elements, %d chips\n", numElements, nchips);
    fprintf(fp, "ZMQ target: %s (control port %d, data port %d)\n",
            ipAddress, ZMQ_CONTROL_PORT, ZMQ_DATA_PORT);
    fprintf(fp, "ZMQ initialized: %s\n", zmqInitialized ? "Yes" : "No");
    fprintf(fp, "Acquisition: %s\n", acquisitionRunning ? "Running" : "Idle");

    ADDriver::report(fp, details);
}

//===========================================================================//

asynStatus germaniumDetector::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                    const char **pptypeName, size_t *psize)
{
    return ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
}

//===========================================================================//

asynStatus germaniumDetector::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    uint32_t regVal = 0;

    if (function == GermaniumVER)
    {
        status = zmqRegisterRead(VERSIONREG, &regVal);
        if (status == asynSuccess)
        {
            *value = static_cast<epicsInt32>(regVal);
            setIntegerParam(GermaniumVER, *value);
        }
    }
    else if (function == GermaniumDETTYPE)
    {
        status = zmqRegisterRead(DETECTOR_TYPE, &regVal);
        if (status == asynSuccess)
        {
            *value = static_cast<epicsInt32>(regVal);
            setIntegerParam(GermaniumDETTYPE, *value);
        }
    }
    else if (function == GermaniumTPAMP_RBV)
    {
        status = zmqRegisterRead(MARS_CALPULSE, &regVal);
        if (status == asynSuccess)
        {
            *value = static_cast<epicsInt32>(regVal);
            setIntegerParam(GermaniumTPAMP_RBV, *value);
        }
    }
    else if (function == GermaniumTPFRQ_RBV)
    {
        status = zmqRegisterRead(CALPULSE_RATE, &regVal);
        if (status == asynSuccess)
        {
            *value = static_cast<epicsInt32>(regVal);
            setIntegerParam(GermaniumTPFRQ_RBV, *value);
        }
    }
    else if (function == GermaniumTPCNT_RBV)
    {
        status = zmqRegisterRead(CALPULSE_CNT, &regVal);
        if (status == asynSuccess)
        {
            *value = static_cast<epicsInt32>(regVal);
            setIntegerParam(GermaniumTPCNT_RBV, *value);
        }
    }
    else if (function == GermaniumTPENB_RBV)
    {
        status = zmqRegisterRead(CALPULSE_MODE, &regVal);
        if (status == asynSuccess)
        {
            *value = static_cast<epicsInt32>(regVal);
            setIntegerParam(GermaniumTPENB_RBV, *value);
        }
    }
    else if (function == GermaniumPLDEL_RBV)
    {
        status = zmqRegisterRead(MARS_PIPE_DELAY, &regVal);
        if (status == asynSuccess)
        {
            *value = static_cast<epicsInt32>(regVal);
            setIntegerParam(GermaniumPLDEL_RBV, *value);
        }
    }
    else if (function == GermaniumRODEL_RBV)
    {
        status = zmqRegisterRead(MARS_RDOUT_ENB, &regVal);
        if (status == asynSuccess)
        {
            *value = static_cast<epicsInt32>(regVal);
            setIntegerParam(GermaniumRODEL_RBV, *value);
        }
    }
    else if (function == GermaniumMODE)
    {
        status = zmqRegisterRead(COUNT_MODE, &regVal);
        if (status == asynSuccess)
        {
            int modeReg = regVal ? 1 : 0;
            *value = modeReg;
            setIntegerParam(GermaniumMODE, modeReg);
        }
    }
    else
    {
        // Fall through to base class for standard AD parameters
        status = ADDriver::readInt32(pasynUser, value);
        return status;
    }

    if (status == asynSuccess)
        callParamCallbacks();

    return status;
}

//===========================================================================//

asynStatus germaniumDetector::readInt8Array(asynUser *pasynUser, epicsInt8 *value,
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

asynStatus germaniumDetector::writeInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                     size_t nElements)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    size_t count = std::min(nElements, static_cast<size_t>(numElements));

    if (function == GermaniumCHEN)
    {
        for (size_t i = 0; i < count; i++)
            zmqMarsSetChannel(i, MARS_CH_CHEN, (value[i] != 0) ? 1 : 0);
        status = zmqMarsLoad((1U << nchips) - 1);
    }
    else if (function == GermaniumTSEN)
    {
        for (size_t i = 0; i < count; i++)
            zmqMarsSetChannel(i, MARS_CH_TSEN, (value[i] != 0) ? 1 : 0);
        status = zmqMarsLoad((1U << nchips) - 1);
    }
    else if (function == GermaniumTHTR)
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
