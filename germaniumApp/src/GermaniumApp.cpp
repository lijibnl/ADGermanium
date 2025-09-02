/**
 * @file GermaniumApp.cpp
 * @brief Definitions for EPICS usage.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "Germanium.hpp"
#include <iocsh.h>
#include <epicsExit.h>
#include <epicsExport.h>

//===========================================================================//

extern "C" {

// IOC shell function for creating Germanium detector
static const iocshArg germaniumConfigArg0 = {"portName", iocshArgString};
static const iocshArg germaniumConfigArg1 = {"numElements", iocshArgInt}; 
static const iocshArg germaniumConfigArg2 = {"ipAddress", iocshArgString};
static const iocshArg germaniumConfigArg3 = {"maxAddr", iocshArgInt};
static const iocshArg germaniumConfigArg4 = {"numParams", iocshArgInt};
static const iocshArg germaniumConfigArg5 = {"maxBuffers", iocshArgInt};
static const iocshArg germaniumConfigArg6 = {"maxMemory", iocshArgInt};

//===========================================================================//

static const iocshArg * const germaniumConfigArgs[] = {
    &germaniumConfigArg0,
    &germaniumConfigArg1, 
    &germaniumConfigArg2,
    &germaniumConfigArg3,
    &germaniumConfigArg4,
    &germaniumConfigArg5,
    &germaniumConfigArg6
};

//===========================================================================//

static const iocshFuncDef germaniumConfigFuncDef = {
    "germaniumConfig", 7, germaniumConfigArgs
};

//===========================================================================//

static void germaniumConfigCallFunc(const iocshArgBuf *args)
{
    const char *portName = args[0].sval;
    int numElements = args[1].ival;
    const char *ipAddress = args[2].sval;
    int maxAddr = args[3].ival;
    int numParams = args[4].ival;
    int maxBuffers = args[5].ival; 
    int maxMemory = args[6].ival;
    
    // Create the Germanium detector driver
    // areaDetector R3-12-1 parameters
    new Germanium(portName, numElements, ipAddress,
                  maxAddr, numParams, maxBuffers, maxMemory,
                  asynInt32Mask | asynFloat64Mask | asynOctetMask | 
                  asynInt32ArrayMask | asynFloat64ArrayMask,
                  asynInt32Mask | asynFloat64Mask | asynOctetMask |
                  asynInt32ArrayMask | asynFloat64ArrayMask,
                  ASYN_CANBLOCK, 1, 0, 0);
}

//===========================================================================//

static void germaniumRegister(void)
{
    iocshRegister(&germaniumConfigFuncDef, germaniumConfigCallFunc);
}

//===========================================================================//

epicsExportRegistrar(germaniumRegister);

//===========================================================================//

} // extern "C"

//===========================================================================//

