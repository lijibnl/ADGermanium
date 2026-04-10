/**
 * @file germaniumDetector.cpp
 * @brief Constructor, parameter creation and initialization for germaniumDetector
 *        areaDetector driver (ZMQ version).
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "germaniumDetector.hpp"
#include <cstdlib>
#include <cstring>
#include <algorithm>

//===========================================================================//

germaniumDetector::germaniumDetector( const char *portName
                                    , int numElements
                                    , const char *ipAddress
                                    , int maxAddr
                                    , int numParams
                                    , int maxBuffers
                                    , size_t maxMemory
                                    , int interfaceMask
                                    , int interruptMask
                                    , int asynFlags
                                    , int autoConnect
                                    , int priority
                                    , int stackSize
                                    )
                                    : ADDriver( portName
                                              , maxAddr
                                              , numParams
                                              , maxBuffers
                                              , maxMemory
                                              , interfaceMask
                                              , interruptMask
                                              , asynFlags
                                              , autoConnect
                                              , priority
                                              , stackSize
                                              )
                                    , zmqContext(nullptr)
                                    , zmqControlSocket(nullptr)
                                    , zmqDataSocket(nullptr)
                                    , zmqMutex(nullptr)
                                    , zmqInitialized(false)
                                    , plUdpSocket(-1)
                                    , plUdpInitialized(false)
                                    , numElements(numElements)
                                    , nchips(6)
                                    , zmqDataThreadId(nullptr)
                                    , plUdpDataThreadId(nullptr)
                                    , dataProcessingThreadId(nullptr)
                                    , dataWriteThreadId(nullptr)
                                    , threadsRunning(false)
                                    , dataAvailable(nullptr)
                                    , evttot(0)
                                    , acquisitionRunning(false)
                                    , fileWritingEnabled(false)
                                    , currentFileHandle(-1)
                                    , currentFileSize(0)
                                    , currentSegmentNumber(0)
                                    , totalBytesWritten(0)
                                    , totalFilesWritten(0)
                                    , dataQueue(nullptr)
                                    , dataQueueHead(0)
                                    , dataQueueTail(0)
                                    , dataWriteAvailable(nullptr)
                                    , mcaData(nullptr)
                                    , tdcData(nullptr)
                                    , countRates(nullptr)
                                    , totalCounts(nullptr)
{
    strncpy(this->ipAddress, ipAddress, sizeof(this->ipAddress) - 1);
    this->ipAddress[sizeof(this->ipAddress) - 1] = '\0';

    // Set chip count based on element count
    switch (numElements)
    {
        case 96:  nchips = 3;  break;
        case 192: nchips = 6;  break;
        case 384: nchips = 12; break;
        default:
            printf("Germanium: Invalid numElements %d, defaulting to 192\n", numElements);
            this->numElements = 192;
            nchips = 6;
            break;
    }

    printf("Germanium ZMQ detector: %d elements, %d chips, IP: %s\n",
           this->numElements, nchips, this->ipAddress);

    allocateDataArrays();
    createGermaniumParameters();
    setGermaniumInitialValues();

    // Initialize ZMQ communication
    if (initializeZmq())
    {
        dataWriteAvailable = epicsEventCreate(epicsEventEmpty);
        dataAvailable      = epicsEventCreate(epicsEventEmpty);

        threadsRunning = true;

        zmqDataThreadId = epicsThreadCreate( "GermaniumZmqData"
                                           , epicsThreadPriorityHigh
                                           , epicsThreadGetStackSize(epicsThreadStackMedium)
                                           , zmqDataThreadC
                                           , this
                                           );

        dataProcessingThreadId = epicsThreadCreate( "GermaniumDataProc"
                                                  , epicsThreadPriorityMedium
                                                  , epicsThreadGetStackSize(epicsThreadStackMedium)
                                                  , dataProcessingThreadC
                                                  , this
                                                  );

        dataWriteThreadId = epicsThreadCreate( "GermaniumDataWrite"
                                             , epicsThreadPriorityMedium
                                             , epicsThreadGetStackSize(epicsThreadStackMedium)
                                             , dataWriteThreadC
                                             , this
                                             );

        printf("Germanium: ZMQ and processing threads started\n");
    }
    else
    {
        printf("Germanium: Failed to initialize ZMQ communication\n");
    }

    // Optionally initialize PL UDP socket for raw data reception
    if (initializePlUdpSocket())
    {
        plUdpDataThreadId = epicsThreadCreate( "GermaniumPlUdp"
                                             , epicsThreadPriorityHigh
                                             , epicsThreadGetStackSize(epicsThreadStackMedium)
                                             , plUdpDataThreadC
                                             , this
                                             );
        printf("Germanium: PL UDP data thread started\n");
    }

    printf("Germanium ZMQ detector driver initialized\n");
}

//===========================================================================//

germaniumDetector::~germaniumDetector()
{
    threadsRunning = false;
    acquisitionRunning = false;
    fileWritingEnabled = false;

    closeCurrentDataFile();
    closeZmq();
    closePlUdpSocket();

    delete[] dataQueue;
    dataQueue = nullptr;
    delete[] mcaData;
    mcaData = nullptr;
    delete[] tdcData;
    tdcData = nullptr;
    delete[] countRates;
    countRates = nullptr;
    delete[] totalCounts;
    totalCounts = nullptr;

    if (dataWriteAvailable)
    {
        epicsEventDestroy(dataWriteAvailable);
        dataWriteAvailable = nullptr;
    }
    if (dataAvailable)
    {
        epicsEventDestroy(dataAvailable);
        dataAvailable = nullptr;
    }

    printf("Germanium ZMQ detector driver destroyed\n");
}

//===========================================================================//

void germaniumDetector::createGermaniumParameters()
{
    createParam(GermaniumVersString,    asynParamInt32, &GermaniumVER);
    createParam(GermaniumDetTypeString, asynParamInt32, &GermaniumDETTYPE);

    createParam(GermaniumMcaString,    asynParamInt32Array,   &GermaniumMCA);
    createParam(GermaniumTdcString,    asynParamInt32Array,   &GermaniumTDC);
    createParam(GermaniumSpctString,   asynParamInt32Array,   &GermaniumSPCT);
    createParam(GermaniumSpctxString,  asynParamFloat64Array, &GermaniumSPCTX);
    createParam(GermaniumIntensString, asynParamInt32Array,   &GermaniumINTENS);

    createParam(GermaniumExsizeString, asynParamInt32, &GermaniumEXSIZE);
    createParam(GermaniumEysizeString, asynParamInt32, &GermaniumEYSIZE);
    createParam(GermaniumTxsizeString, asynParamInt32, &GermaniumTXSIZE);
    createParam(GermaniumTysizeString, asynParamInt32, &GermaniumTYSIZE);

    createParam(GermaniumIpaddrString,    asynParamOctet, &GermaniumIPADDR);
    createParam(GermaniumIpaddrRbvString, asynParamOctet, &GermaniumIPADDR_RBV);

    createParam(GermaniumFnamString,  asynParamOctet, &GermaniumFNAM);
    createParam(GermaniumCalfString,  asynParamOctet, &GermaniumCALF);
    createParam(GermaniumDirString,   asynParamOctet, &GermaniumDIR);
    createParam(GermaniumFsizeString, asynParamInt32, &GermaniumFSIZE);

    createParam(GermaniumFreqString, asynParamFloat64, &GermaniumFREQ);
    createParam(GermaniumCntString,  asynParamInt32,   &GermaniumCNT);
    createParam(GermaniumPcntString, asynParamInt32,   &GermaniumPCNT);
    createParam(GermaniumContString, asynParamInt32,   &GermaniumCONT);
    createParam(GermaniumModeString, asynParamInt32,   &GermaniumMODE);

    createParam(GermaniumRateString, asynParamFloat64, &GermaniumRATE);
    createParam(GermaniumRat1String, asynParamFloat64, &GermaniumRAT1);

    createParam(GermaniumDlyString,  asynParamFloat64, &GermaniumDLY);
    createParam(GermaniumDly1String, asynParamFloat64, &GermaniumDLY1);

    createParam(GermaniumTpString,  asynParamFloat64, &GermaniumTP);
    createParam(GermaniumTp1String, asynParamFloat64, &GermaniumTP1);
    createParam(GermaniumPr1String, asynParamInt32,   &GermaniumPR1);

    createParam(GermaniumSsString, asynParamInt32,   &GermaniumSS);
    createParam(GermaniumUsString, asynParamInt32,   &GermaniumUS);
    createParam(GermaniumTString,  asynParamFloat64, &GermaniumT);

    createParam(GermaniumRunnoString,    asynParamInt32, &GermaniumRUNNO);
    createParam(GermaniumPldelString,    asynParamInt32, &GermaniumPLDEL);
    createParam(GermaniumPldelRbvString, asynParamInt32, &GermaniumPLDEL_RBV);
    createParam(GermaniumRodelString,    asynParamInt32, &GermaniumRODEL);
    createParam(GermaniumRodelRbvString, asynParamInt32, &GermaniumRODEL_RBV);

    createParam(GermaniumFverString, asynParamInt32, &GermaniumFVER);
    createParam(GermaniumCardString, asynParamInt32, &GermaniumCARD);

    createParam(GermaniumNelmString,   asynParamInt32, &GermaniumNELM);
    createParam(GermaniumNchString,    asynParamInt32, &GermaniumNCH);
    createParam(GermaniumNchipsString, asynParamInt32, &GermaniumNCHIPS);
    createParam(GermaniumChanString,   asynParamInt32, &GermaniumCHAN);
    createParam(GermaniumChipString,   asynParamInt32, &GermaniumCHIP);

    createParam(GermaniumShptString, asynParamInt32, &GermaniumSHPT);
    createParam(GermaniumGainString, asynParamInt32, &GermaniumGAIN);
    createParam(GermaniumPolString,  asynParamInt32, &GermaniumPOL);
    createParam(GermaniumEblkString, asynParamInt32, &GermaniumEBLK);

    createParam(GermaniumGmonString,  asynParamInt32, &GermaniumGMON);
    createParam(GermaniumMonchString, asynParamInt32, &GermaniumMONCH);
    createParam(GermaniumLoaoString,  asynParamInt32, &GermaniumLOAO);

    createParam(GermaniumPuenString, asynParamInt32, &GermaniumPUEN);
    createParam(GermaniumMfsString,  asynParamInt32, &GermaniumMFS);

    createParam(GermaniumTdsString, asynParamInt32, &GermaniumTDS);
    createParam(GermaniumTdmString, asynParamInt32, &GermaniumTDM);

    createParam(GermaniumTpampString, asynParamInt32, &GermaniumTPAMP);
    createParam(GermaniumTpfrqString, asynParamInt32, &GermaniumTPFRQ);
    createParam(GermaniumTpcntString, asynParamInt32, &GermaniumTPCNT);
    createParam(GermaniumTpenbString, asynParamInt32, &GermaniumTPENB);
    // RBV parameters for test pulse
    createParam("GERMANIUM_TPAMP_RBV", asynParamInt32, &GermaniumTPAMP_RBV);
    createParam("GERMANIUM_TPFRQ_RBV", asynParamInt32, &GermaniumTPFRQ_RBV);
    createParam("GERMANIUM_TPCNT_RBV", asynParamInt32, &GermaniumTPCNT_RBV);
    createParam("GERMANIUM_TPENB_RBV", asynParamInt32, &GermaniumTPENB_RBV);

    createParam(GermaniumChenString, asynParamInt8Array,    &GermaniumCHEN);
    createParam(GermaniumTsenString, asynParamInt8Array,    &GermaniumTSEN);
    createParam(GermaniumThtrString, asynParamInt8Array,    &GermaniumTHTR);
    createParam(GermaniumPutrString, asynParamInt8Array,    &GermaniumPUTR);
    createParam(GermaniumChenSelString, asynParamInt32, &GermaniumCHEN_SEL);
    createParam(GermaniumChenAllString, asynParamInt32, &GermaniumCHEN_ALL);
    createParam(GermaniumTsenSelString, asynParamInt32, &GermaniumTSEN_SEL);
    createParam(GermaniumTsenAllString, asynParamInt32, &GermaniumTSEN_ALL);
    createParam(GermaniumSlpString,  asynParamFloat64Array, &GermaniumSLP);
    createParam(GermaniumOffsString, asynParamFloat64Array, &GermaniumOFFS);
    createParam(GermaniumThrshString, asynParamInt32Array,  &GermaniumTHRSH);

    createParam("GERMANIUM_CLRE", asynParamInt32, &GermaniumCLRE);
    createParam("GERMANIUM_CLRM", asynParamInt32, &GermaniumCLRM);
    createParam("GERMANIUM_CLRT", asynParamInt32, &GermaniumCLRT);
    createParam("GERMANIUM_STRT", asynParamInt32, &GermaniumSTRT);
    createParam("GERMANIUM_STOP", asynParamInt32, &GermaniumSTOP);

    createParam(GermaniumEguString,  asynParamOctet, &GermaniumEGU);
    createParam(GermaniumPrecString, asynParamInt32, &GermaniumPREC);

    createParam(GermaniumCoutString,  asynParamOctet, &GermaniumCOUT);
    createParam(GermaniumCoutpString, asynParamOctet, &GermaniumCOUTP);

    createParam(GermaniumTemp1String,   asynParamFloat64, &GermaniumTEMP1);
    createParam(GermaniumTemp2String,   asynParamFloat64, &GermaniumTEMP2);
    createParam(GermaniumTemp3String,   asynParamFloat64, &GermaniumTEMP3);
    createParam(GermaniumZTempString,   asynParamFloat64, &GermaniumZTEMP);
    createParam(GermaniumHvString,      asynParamFloat64, &GermaniumHV);
    createParam(GermaniumHvRbvString,   asynParamFloat64, &GermaniumHV_RBV);
    createParam(GermaniumHvCurrString,  asynParamFloat64, &GermaniumHV_CURR);
    createParam(GermaniumP1String,       asynParamFloat64, &GermaniumP1);
    createParam(GermaniumP2String,       asynParamFloat64, &GermaniumP2);
    createParam(GermaniumP1CurrString,   asynParamFloat64, &GermaniumP1_CURR);
    createParam(GermaniumP2CurrString,   asynParamFloat64, &GermaniumP2_CURR);
    createParam(GermaniumAdc0ClkSkewString, asynParamInt32, &GermaniumADC0_CLK_SKEW);
    createParam(GermaniumAdc1ClkSkewString, asynParamInt32, &GermaniumADC1_CLK_SKEW);
    createParam(GermaniumAdc2ClkSkewString, asynParamInt32, &GermaniumADC2_CLK_SKEW);
}

//===========================================================================//

void germaniumDetector::setGermaniumInitialValues()
{
    setIntegerParam(GermaniumEXSIZE, SPECTRUM_SIZE);
    setIntegerParam(GermaniumEYSIZE, numElements);
    setIntegerParam(GermaniumTXSIZE, TDC_SIZE);
    setIntegerParam(GermaniumTYSIZE, numElements);

    setStringParam(GermaniumIPADDR, ipAddress);
    setStringParam(GermaniumDIR, "/tmp/germanium/");
    setStringParam(GermaniumFNAM, "germanium_data");
    setIntegerParam(GermaniumFSIZE, 100);  // 100 MB default

    setDoubleParam(GermaniumFREQ, 25.0e6); // 25 MHz (FPGA clock)
    setDoubleParam(GermaniumRATE, 2.0);
    setDoubleParam(GermaniumTP1, 1.0);

    setIntegerParam(GermaniumNELM, numElements);
    setIntegerParam(GermaniumNCHIPS, nchips);
    setIntegerParam(GermaniumPLDEL, 72);
    setIntegerParam(GermaniumRODEL, 15);

    setIntegerParam(GermaniumCNT, 0);
    setIntegerParam(GermaniumCONT, 0);
    setIntegerParam(GermaniumMODE, 0);
    setIntegerParam(GermaniumSHPT, 1);
    setIntegerParam(GermaniumGAIN, 0);
    setIntegerParam(GermaniumPOL, 1);
    setIntegerParam(GermaniumEBLK, 1);
    setIntegerParam(GermaniumGMON, 0);
    setIntegerParam(GermaniumLOAO, 1);
    setIntegerParam(GermaniumPUEN, 0);
    setIntegerParam(GermaniumMFS, 0);
    setIntegerParam(GermaniumTDS, 0);
    setIntegerParam(GermaniumTDM, 0);
    setIntegerParam(GermaniumTPENB, 0);

    setStringParam(GermaniumEGU, "counts");
    setIntegerParam(GermaniumPREC, 0);

    // Placeholders — I2C sensors not available via ZMQ yet
    setDoubleParam(GermaniumTEMP1, 0.0);
    setDoubleParam(GermaniumTEMP2, 0.0);
    setDoubleParam(GermaniumTEMP3, 0.0);
    setDoubleParam(GermaniumZTEMP, 0.0);
    setDoubleParam(GermaniumHV, 0.0);
    setDoubleParam(GermaniumHV_RBV, 0.0);
    setDoubleParam(GermaniumHV_CURR, 0.0);
    setDoubleParam(GermaniumP1, 0.0);
    setDoubleParam(GermaniumP2, 0.0);
    setDoubleParam(GermaniumP1_CURR, 0.0);
    setDoubleParam(GermaniumP2_CURR, 0.0);

    callParamCallbacks();
}

//===========================================================================//

void germaniumDetector::allocateDataArrays()
{
    // Flat atomic arrays — safe for concurrent access from multiple
    // producer threads (zmqData, plUdp) and the EPICS read thread.
    size_t mcaTotal = static_cast<size_t>(numElements) * SPECTRUM_SIZE;
    size_t tdcTotal = static_cast<size_t>(numElements) * TDC_SIZE;

    mcaData    = new std::atomic<uint32_t>[mcaTotal];
    tdcData    = new std::atomic<uint32_t>[tdcTotal];
    countRates = new std::atomic<uint32_t>[numElements];
    totalCounts= new std::atomic<uint64_t>[numElements];

    for (size_t i = 0; i < mcaTotal; i++)
        mcaData[i].store(0, std::memory_order_relaxed);
    for (size_t i = 0; i < tdcTotal; i++)
        tdcData[i].store(0, std::memory_order_relaxed);
    for (int i = 0; i < numElements; i++)
    {
        countRates[i].store(0, std::memory_order_relaxed);
        totalCounts[i].store(0, std::memory_order_relaxed);
    }
    evttot.store(0, std::memory_order_relaxed);

    // Allocate lock-free block queue
    dataQueue = new DataBlock[DATA_QUEUE_CAPACITY];
    for (int i = 0; i < DATA_QUEUE_CAPACITY; i++)
        dataQueue[i].state.store(DATA_BLOCK_FREE, std::memory_order_relaxed);

    printf("Germanium: Allocated data arrays for %d elements\n", numElements);
}

//===========================================================================//

void germaniumDetector::processPhotonEvent(int element, int energy, int tdValue)
{
    if (element < 0 || element >= numElements) return;
    if (energy < 0 || energy >= SPECTRUM_SIZE) return;
    if (tdValue < 0 || tdValue >= TDC_SIZE) return;

    mcaData[element * SPECTRUM_SIZE + energy].fetch_add(1, std::memory_order_relaxed);
    tdcData[element * TDC_SIZE + tdValue].fetch_add(1, std::memory_order_relaxed);
    countRates[element].fetch_add(1, std::memory_order_relaxed);
    totalCounts[element].fetch_add(1, std::memory_order_relaxed);
    evttot.fetch_add(1, std::memory_order_relaxed);
}

//===========================================================================//

void germaniumDetector::clearSpectra()
{
    size_t mcaTotal = static_cast<size_t>(numElements) * SPECTRUM_SIZE;
    size_t tdcTotal = static_cast<size_t>(numElements) * TDC_SIZE;

    for (size_t i = 0; i < mcaTotal; i++)
        mcaData[i].store(0, std::memory_order_relaxed);
    for (size_t i = 0; i < tdcTotal; i++)
        tdcData[i].store(0, std::memory_order_relaxed);
    for (int i = 0; i < numElements; i++)
    {
        countRates[i].store(0, std::memory_order_relaxed);
        totalCounts[i].store(0, std::memory_order_relaxed);
    }
    evttot.store(0, std::memory_order_relaxed);
}

//===========================================================================//
