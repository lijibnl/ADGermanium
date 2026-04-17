/**
 * @file GermaniumDetectorApp.cpp
 * @brief EPICS IOC shell registration for GermaniumDetector (ZMQ version).
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "GermaniumDetector.hpp"
#include <iocsh.h>
#include <epicsExport.h>

//===========================================================================//

extern "C" {

static const iocshArg germaniumConfigArg0 = {"portName",    iocshArgString};
static const iocshArg germaniumConfigArg1 = {"numElements", iocshArgInt};
static const iocshArg germaniumConfigArg2 = {"ipAddress",   iocshArgString};
static const iocshArg germaniumConfigArg3 = {"maxAddr",     iocshArgInt};
static const iocshArg germaniumConfigArg4 = {"numParams",   iocshArgInt};
static const iocshArg germaniumConfigArg5 = {"maxBuffers",  iocshArgInt};
static const iocshArg germaniumConfigArg6 = {"maxMemory",   iocshArgInt};

static const iocshArg * const germaniumConfigArgs[] = {
    &germaniumConfigArg0, &germaniumConfigArg1, &germaniumConfigArg2,
    &germaniumConfigArg3, &germaniumConfigArg4, &germaniumConfigArg5,
    &germaniumConfigArg6
};

static const iocshFuncDef germaniumConfigFuncDef = {
    "germaniumConfig", 7, germaniumConfigArgs
};

//===========================================================================//

static void germaniumConfigCallFunc(const iocshArgBuf *args)
{
    new GermaniumDetector( args[0].sval         // portName
                         , args[1].ival         // numElements
                         , args[2].sval         // ipAddress
                         , args[3].ival         // maxAddr
                         , args[4].ival         // numParams
                         , args[5].ival         // maxBuffers
                         , args[6].ival         // maxMemory
                         , asynInt32Mask | asynFloat64Mask | asynOctetMask |
                           asynInt32ArrayMask | asynFloat64ArrayMask |
                           asynInt8ArrayMask
                         , asynInt32Mask | asynFloat64Mask | asynOctetMask |
                           asynInt32ArrayMask | asynFloat64ArrayMask |
                           asynInt8ArrayMask
                         , ASYN_CANBLOCK
                         , 1    // autoConnect
                         , 0    // priority
                         , 0    // stackSize
                         );
}

//===========================================================================//

static void germaniumRegister(void)
{
    iocshRegister(&germaniumConfigFuncDef, germaniumConfigCallFunc);
}

epicsExportRegistrar(germaniumRegister);

} // extern "C"

//===========================================================================//
