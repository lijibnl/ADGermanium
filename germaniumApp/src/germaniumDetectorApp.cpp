/**
 * @file germaniumDetectorApp.cpp
 * @brief Definitions for EPICS usage.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "germaniumDetector.hpp"
#include <iocsh.h>
#include <epicsExit.h>
#include <epicsExport.h>

#include "errlog.h"

//===========================================================================//

extern "C" {

// IOC shell function for creating Germanium detector
static const iocshArg germaniumConfigArg0  = {"portName", iocshArgString};
static const iocshArg germaniumConfigArg2  = {"numElements", iocshArgInt}; 
static const iocshArg germaniumConfigArg1  = {"ipAddress", iocshArgString};
static const iocshArg germaniumConfigArg3  = {"maxAddr", iocshArgInt};
static const iocshArg germaniumConfigArg4  = {"numParams", iocshArgInt};
static const iocshArg germaniumConfigArg5  = {"maxBuffers", iocshArgInt};
static const iocshArg germaniumConfigArg6  = {"maxMemory", iocshArgInt};
static const iocshArg germaniumConfigArg7  = {"mcaAddr", iocshArgInt};
static const iocshArg germaniumConfigArg8  = {"tdcAddr", iocshArgInt};
static const iocshArg germaniumConfigArg9  = {"spctAddr", iocshArgInt};
static const iocshArg germaniumConfigArg10 = {"intensAddr", iocshArgInt};

//===========================================================================//

static const iocshArg * const germaniumConfigArgs[] = {
    &germaniumConfigArg0,
    &germaniumConfigArg1, 
    &germaniumConfigArg2,
    &germaniumConfigArg3,
    &germaniumConfigArg4,
    &germaniumConfigArg5,
    &germaniumConfigArg6,
    &germaniumConfigArg7,
    &germaniumConfigArg8,
    &germaniumConfigArg9,
    &germaniumConfigArg10
};

//===========================================================================//

static const iocshFuncDef germaniumConfigFuncDef = {
    "germaniumConfig", 10, germaniumConfigArgs
};

//===========================================================================//

static void germaniumConfigCallFunc(const iocshArgBuf *args)
{
    const char *portName  = args[0].sval;
    int numElements       = args[2].ival;
    const char *ipAddress = args[1].sval;
    int maxAddr           = args[3].ival;
    int numParams         = args[4].ival;
    int maxBuffers        = args[5].ival; 
    int maxMemory         = args[6].ival;
    int mcaAddr           = args[7].ival;
    int tdcAddr           = args[8].ival;
    int spctAddr          = args[9].ival;
    int intensAddr        = args[10].ival;

    // Create the Germanium detector driver
    // areaDetector R3-12-1 parameters
    new germaniumDetector( portName
                         , numElements
                         , ipAddress
                         , maxAddr
                         , numParams
                         , maxBuffers
                         , maxMemory
                         , mcaAddr
                         , tdcAddr
                         , spctAddr
                         , intensAddr
                         , asynInt32Mask | asynFloat64Mask | asynOctetMask | 
                           asynInt32ArrayMask | asynFloat64ArrayMask | asynDrvUserMask
                         , asynInt32Mask | asynFloat64Mask | asynOctetMask |
                           asynInt32ArrayMask | asynFloat64ArrayMask | asynDrvUserMask
                         , ASYN_CANBLOCK
                         , 1
                         , 0
                         , 0
                         );
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

//extern "C"{
//static void testFunc(const iocshArgBuf *args) {
//    errlogPrintf("testFunc: %d\n", args[0].ival);
//}
//static const iocshArg testArg = {"val", iocshArgInt};
//static const iocshFuncDef testFuncDef = {"testFunc", 1, &testArg};
//static void testRegister(void) { iocshRegister(&testFuncDef, testFunc); }
//epicsExportRegistrar(testRegister);
//}
