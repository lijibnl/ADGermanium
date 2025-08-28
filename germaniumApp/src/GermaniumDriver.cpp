/*
 * GermaniumDriver.cpp
 * asynPortDriver interface implementations for Germanium detector
 * Handles all parameter read/write operations and communication with hardware
 */

#include "Germanium.hpp"
#include "GermaniumTypes.hpp"
#include <algorithm>
#include <cstring>

// asynPortDriver virtual method implementations
asynStatus Germanium::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    // First set the parameter locally
    status = ADDriver::setIntegerParam(function, value);

    if (status == asynSuccess)
    {
        // Send the parameter change to the remote device via UDP
        // Most MARS ASIC parameters are stored in globalstr/channelstr arrays
        // and sent via bulk configuration, not direct register writes
        if (function == GermaniumSHPT)
        {
            // Shaping time - update globalstr for all chips and send bulk config
            for (int chip = 0; chip < nchips; chip++)
            {
                globalstr[chip].st = value;
            }
            status = sendMarsConfiguration();
        }
        else if (function == GermaniumGAIN)
        {
            // Gain setting - update globalstr for all chips and send bulk config
            for (int chip = 0; chip < nchips; chip++)
            {
                globalstr[chip].g = value;
            }
            status = sendMarsConfiguration();
        }
        else if (function == GermaniumPOL)
        {
            // Polarity - update globalstr for all chips and send bulk config
            for (int chip = 0; chip < nchips; chip++)
            {
                globalstr[chip].pol = value;
            }
            status = sendMarsConfiguration();
        }
        else if (function == GermaniumEBLK)
        {
            // Bias current - update globalstr for all chips and send bulk config
            for (int chip = 0; chip < nchips; chip++)
            {
                globalstr[chip].eblk = value;
            }
            status = sendMarsConfiguration();
        }
        else if (function == GermaniumTHRSH)
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
        else if (function == GermaniumGMON)
        {
            // Global monitor mode - implement exact zDDM logic
            // This is complex logic from original special() function
            for (int chip = 0; chip < nchips; chip++) {
                switch(value) {
                case 0: // All monitors off. M0=0, C0-C4=00000
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 1: // All off except this chip temp. M0=0 C0-C4=00100
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 2: // All off except this chip base M0=0 C0-C4=10100
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 3: // All off except this chip thresh M0=0 C0-C4=01100
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 4: // All off except this chip test pulse M0=0 C0-C4=11100
                    globalstr[chip].c = 0;
                    globalstr[chip].m0 = 0;
                    globalstr[chip].saux = 0;
                    break;
                case 5: // All off except this chip channel monitor M0=1; channel number to C0-C4
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
                switch(value) {
                case 1: // Temperature
                    globalstr[currentChip].c = 4;
                    globalstr[currentChip].saux = 1;
                    break;
                case 2: // Baseline
                    globalstr[currentChip].c = 5;
                    globalstr[currentChip].saux = 1;
                    break;
                case 3: // Threshold
                    globalstr[currentChip].c = 6;
                    globalstr[currentChip].saux = 1;
                    break;
                case 4: // Test pulse
                    globalstr[currentChip].c = 7;
                    globalstr[currentChip].saux = 1;
                    break;
                case 5: // Channel monitor
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
        else if (function == GermaniumPUEN)
        {
            // Pileup rejection enable - update globalstr for all chips and send bulk config
            for (int chip = 0; chip < nchips; chip++)
            {
                globalstr[chip].puen = value;
            }
            status = sendMarsConfiguration();
        }
        else if (function == GermaniumMFS)
        {
            // Multi-fire suppression - update globalstr for all chips and send bulk config
            for (int chip = 0; chip < nchips; chip++)
            {
                globalstr[chip].mfs = value;
            }
            status = sendMarsConfiguration();
        }
        else if (function == GermaniumTDS)
        {
            // TDS slope - implement exact zDDM logic from original
            // Original uses complex timing calculation
            int tempVal = value;
            if (tempVal == 0) tempVal = 1; // Prevent divide by zero
            
            // Original calculation: tds = 1.0e6 / (16.0 * tempVal);
            // This gets converted to register value for hardware
            double tds = 1.0e6 / (16.0 * tempVal);
            
            // Update all chips with the calculated TDS value
            for (int chip = 0; chip < nchips; chip++)
            {
                // The original uses tds for all chips
                globalstr[chip].tds = (int)(tds * 10); // Scale for register
            }
            status = sendMarsConfiguration();
        }
        else if (function == GermaniumTDM)
        {
            // TDC mode - update globalstr for all chips and send bulk config
            for (int chip = 0; chip < nchips; chip++)
            {
                globalstr[chip].tdm = value;
            }
            status = sendMarsConfiguration();
        }
        else if (function == GermaniumMONCH)
        {
            // Monitor channel - implement exact zDDM logic
            // Set CHAN field to the current chip * 64 + MONCH value
            int currentChip;
            getIntegerParam(GermaniumCHIP, &currentChip);
            if (currentChip >= 0 && currentChip < nchips) {
                int chanValue = currentChip * 64 + value;
                setIntegerParam(GermaniumCHAN, chanValue);
            }
            
            // Update the spectrum if we're in global monitor mode 5 (channel monitor)
            int gmonMode;
            getIntegerParam(GermaniumGMON, &gmonMode);
            if (gmonMode == 5) {
                // Send configuration to update the hardware monitor channel
                if (currentChip >= 0 && currentChip < nchips) {
                    globalstr[currentChip].c = value;
                    globalstr[currentChip].m0 = 1;
                    globalstr[currentChip].saux = 1;
                    status = sendMarsConfiguration();
                }
            }
        }
        else if (function == GermaniumCHIP)
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
            if (gmonMode == 5) {
                int monch;
                getIntegerParam(GermaniumMONCH, &monch);
                if (value >= 0 && value < nchips) {
                    globalstr[value].c = monch;
                    globalstr[value].m0 = 1;
                    globalstr[value].saux = 1;
                    status = sendMarsConfiguration();
                }
            }
        }
        else if (function == GermaniumCHAN)
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
            if (gmonMode == 5 && chip >= 0 && chip < nchips) {
                globalstr[chip].c = monch;
                globalstr[chip].m0 = 1;
                globalstr[chip].saux = 1;
                status = sendMarsConfiguration();
            }
        }
        else if (function == GermaniumLOAO)
        {
            // Leakage/pulse monitor select - this is a control parameter
            // May be handled through SIM_EVT_SEL or other control registers
            status = udpRegisterWrite(SIM_EVT_SEL, value);
        }
        else if (function == GermaniumTPAMP)
        {
            status = udpRegisterWrite(MARS_CALPULSE, value);
        }
        else if (function == GermaniumTPFRQ)
        {
            status = udpRegisterWrite(CALPULSE_RATE, value);
        }
        else if (function == GermaniumTPCNT)
        {
            status = udpRegisterWrite(CALPULSE_CNT, value);
        }
        else if (function == GermaniumTPENB)
        {
            status = udpRegisterWrite(CALPULSE_MODE, value);
        }
        else if (function == GermaniumCNTS)
        {
            // Counts status - read-only, update from hardware
            // This should be updated by a periodic read task
            status = asynSuccess; // Read-only parameter
        }
        else if (function == GermaniumTIMS)
        {
            // Timer status - read-only, update from hardware
            // This should be updated by a periodic read task
            status = asynSuccess; // Read-only parameter
        }
        else if (function == GermaniumRATE)
        {
            // Rate - read-only, computed from counts and time
            // This should be updated by a periodic read task
            status = asynSuccess; // Read-only parameter
        }
        else if (function == GermaniumFSIZE)
        {
            // File size limit - validate and store
            if (value < 1024*1024) // Minimum 1MB
            {
                printf("Germanium: File size too small, setting to 1MB minimum\n");
                setIntegerParam(GermaniumFSIZE, 1024*1024);
            }
            else if (value > 2*1024*1024*1024) // Maximum 2GB
            {
                printf("Germanium: File size too large, setting to 2GB maximum\n");
                setIntegerParam(GermaniumFSIZE, 2*1024*1024*1024);
            }
            printf("Germanium: Maximum file size set to %d bytes\n", value);
        }
        else if (function == GermaniumCNT)
        {
            // Acquisition control
            if (value == 1)
            { // Start acquisition
                startDataAcquisition();
                status = udpRegisterWrite(COUNT_MODE, 1);
            }
            else
            { // Stop acquisition
                stopDataAcquisition();
                status = udpRegisterWrite(COUNT_MODE, 0);
            }
        }
        else if (function == GermaniumMODE)
        {
            status = udpRegisterWrite(COUNT_MODE, value);
        }
        else if (function == GermaniumCLRE)
        {
            // Clear event - implement exact zDDM logic
            if (value == 1) {
                // Clear the event spectrum data
                status = udpRegisterWrite(CLR_EVT, 1);
                // Reset the value back to 0
                setIntegerParam(GermaniumCLRE, 0);
            }
        }
        else if (function == GermaniumCLRM)
        {
            // Clear monitor - implement exact zDDM logic
            if (value == 1) {
                // Clear the monitor spectrum data
                status = udpRegisterWrite(CLR_MON, 1);
                // Reset the value back to 0
                setIntegerParam(GermaniumCLRM, 0);
            }
        }
        else if (function == GermaniumCLRT)
        {
            // Clear timer - implement exact zDDM logic
            if (value == 1) {
                // Clear the timer
                status = udpRegisterWrite(CLR_TIM, 1);
                // Reset the value back to 0
                setIntegerParam(GermaniumCLRT, 0);
            }
        }
        else if (function == GermaniumSTRT)
        {
            // Start acquisition - implement exact zDDM logic
            if (value == 1) {
                // Start data acquisition
                status = udpRegisterWrite(STRT, 1);
                // Reset the value back to 0
                setIntegerParam(GermaniumSTRT, 0);
            }
        }
        else if (function == GermaniumSTOP)
        {
            // Stop acquisition - implement exact zDDM logic
            if (value == 1) {
                // Stop data acquisition
                status = udpRegisterWrite(STOP, 1);
                // Reset the value back to 0
                setIntegerParam(GermaniumSTOP, 0);
            }
        }
        else if (function == GermaniumPLDEL)
        {
            status = udpRegisterWrite(MARS_PIPE_DELAY, value);
        }
        else if (function == GermaniumRODEL)
        {
            // Readout delay - may be part of MARS configuration
            status = udpRegisterWrite(MARS_RDOUT_ENB, value);
        }
        // Add more parameter mappings as needed

        if (status == asynSuccess)
        {
            callParamCallbacks();
        }
        else
        {
            printf("Germanium: Failed to send parameter %d to device\n", function);
        }
    }

    return status;
}

asynStatus Germanium::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    // First set the parameter locally
    status = ADDriver::setDoubleParam(function, value);

    if (status == asynSuccess)
    {
        // Send the parameter change to the remote device via UDP
        if (function == GermaniumFREQ)
        {
            // Time base frequency - may not have direct register in pl.h
            // Could be derived from COUNT_TIME registers
            uint32_t regValue = (uint32_t)(value / 1000.0); // Example scaling
            status = udpRegisterWrite(COUNT_TIME_LO, regValue & 0xFFFF);
            if (status == asynSuccess)
            {
                status = udpRegisterWrite(COUNT_TIME_HI, (regValue >> 16) & 0xFFFF);
            }
        }
        else if (function == GermaniumTP)
        {
            // Time preset - use COUNT_TIME registers
            uint32_t ticks = (uint32_t)(value * 1000000); // Convert seconds to microseconds
            status = udpRegisterWrite(COUNT_TIME_LO, ticks & 0xFFFF);
            if (status == asynSuccess)
            {
                status = udpRegisterWrite(COUNT_TIME_HI, (ticks >> 16) & 0xFFFF);
            }
        }
        else if (function == GermaniumTP1)
        {
            // Auto time preset (use same registers for now)
            uint32_t ticks = (uint32_t)(value * 1000000);
            status = udpRegisterWrite(COUNT_TIME_LO, ticks & 0xFFFF);
            if (status == asynSuccess)
            {
                status = udpRegisterWrite(COUNT_TIME_HI, (ticks >> 16) & 0xFFFF);
            }
        }
        else if (function == GermaniumDLY)
        {
            // Delay setting - could be TD_CAL (Time Delay Calibration)
            uint32_t delayReg = (uint32_t)(value * 1000); // Convert to appropriate units
            status = udpRegisterWrite(TD_CAL, delayReg);
        }
        else if (function == GermaniumRATE)
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
        // Add more float parameter mappings as needed

        if (status == asynSuccess)
        {
            callParamCallbacks();
        }
        else
        {
            printf("Germanium: Failed to send float parameter %d to device\n", function);
        }
    }

    return status;
}

asynStatus Germanium::writeOctet(asynUser *pasynUser, const char *value,
                                 size_t maxChars, size_t *nActual)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    // First set the parameter locally
    status = ADDriver::setStringParam(function, value);

    if (status == asynSuccess)
    {
        // Send string parameters to device if needed
        if (function == GermaniumFNAM)
        {
            // Filename - might trigger file operations on remote device
            status = udpSendString(UDP_CMD_SET_FILENAME, value);
        }
        else if (function == GermaniumCALF)
        {
            // Calibration filename - load calibration on remote device
            status = udpSendString(UDP_CMD_LOAD_CALIBRATION, value);
        }
        else if (function == GermaniumDIR)
        {
            // Data directory - create if it doesn't exist
            createDataDirectory();
            printf("Germanium: Data directory set to %s\n", value);
        }
        else if (function == GermaniumIPADDR)
        {
            // IP address change - might need to reconnect
            printf("Germanium: IP address change to %s - restart required\n", value);
            // Don't send this via UDP as it would change our connection
        }

        if (status == asynSuccess)
        {
            callParamCallbacks();
        }
        else
        {
            printf("Germanium: Failed to send string parameter %d to device\n", function);
        }
    }

    *nActual = strlen(value);
    return status;
}

asynStatus Germanium::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                     size_t nElements, size_t *nIn)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    // Handle array reads for MCA, TDC, etc. using vectors
    if (function == GermaniumMCA)
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
    else if (function == GermaniumTDC)
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
    else if (function == GermaniumSPCT || function == GermaniumINTENS)
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
        status = asynError;
    }

    return status;
}

asynStatus Germanium::writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                      size_t nElements)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;

    // Handle array writes for configuration arrays via UDP
    if (function == GermaniumCHEN)
    {
        // Channel enable array - convert to uint8 and send
        uint8_t *chenArray = new uint8_t[nElements];
        for (size_t i = 0; i < nElements; i++)
        {
            chenArray[i] = (value[i] != 0) ? 1 : 0;
        }
        status = udpWriteIntArray(UDP_CMD_CHANNEL_ENABLE, chenArray,
                                  nElements, 0);
        delete[] chenArray;
    }
    else if (function == GermaniumTHRSH)
    {
        // Threshold array (per chip) - send as uint32 array
        status = udpWriteIntArray(UDP_CMD_THRESHOLD_ARRAY, value,
                                  nElements * sizeof(epicsInt32), 0);
    }
    else if (function == GermaniumSLP || function == GermaniumOFFS)
    {
        // Calibration arrays - send as is
        status = udpWriteIntArray(UDP_CMD_CALIBRATION_ARRAY, value,
                                  nElements * sizeof(epicsInt32), function);
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

// ADDriver virtual method implementations
asynStatus Germanium::readNDArray(asynUser *pasynUser, epicsInt32 *value,
                                  size_t nElements, size_t *nIn)
{
    // This method reads detector data as NDArrays for areaDetector framework
    // Implementation would read from hardware and create NDArray
    *nIn = 0;
    return asynSuccess;
}

void Germanium::report(FILE *fp, int details)
{
    fprintf(fp, "Germanium detector: %d elements, %d chips\n", numElements, nchips);
    fprintf(fp, "IP Address: %s\n", ipAddress);

    if (details > 1)
    {
        fprintf(fp, "Device FD: %d\n", device_fd);
        fprintf(fp, "Acquisition running: %s\n", acquisitionRunning ? "Yes" : "No");
    }

    // Call base class report
    ADDriver::report(fp, details);
}

asynStatus Germanium::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                    const char **pptypeName, size_t *psize)
{
    // Handle driver-specific parameter creation
    return ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
}
