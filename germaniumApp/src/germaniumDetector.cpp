/**
 * @file germaniumDetector.cpp
 * @brief Constructor, parameter creation and initialization for germaniumDetector areaDetector driver.
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
#include "NDArray.h"
#include "errlog.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

//===========================================================================//
// Constructor
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
                                    , udpControlSocket(-1)
                                    , udpDataSocket(-1)
                                    , udpInitialized(false)
                                    , zDDMWdTimerQ(nullptr)
                                    , TPgenTimerQ(nullptr)
                                    , numElements(numElements)
                                    , controlPort(GERMANIUM_CONTROL_PORT)
                                    , dataPort(GERMANIUM_DATA_PORT)
                                    , udpControlThreadId(nullptr)
                                    , udpDataThreadId(nullptr)
                                    , dataProcessingThreadId(nullptr)
                                    , threadsRunning(false)
                                    , udpMutex(nullptr)
                                    , dataAvailable(nullptr)
                                    , udpDataBuffer(nullptr)
                                    , dataBufferSize(0)
                                    , evttot(0)
                                    , framestat(0)
                                    , fileWritingEnabled(false)
                                    , currentFileHandle(-1)
                                    , currentFileSize(0)
                                    , currentSegmentNumber(0)
                                    , totalBytesWritten(0)
                                    , totalFilesWritten(0)
                                    , writeBufferHead(0)
                                    , writeBufferTail(0)
                                    , writeBufferCount(0)
                                    , writeBufferMutex(nullptr)
                                    , dataWriteAvailable(nullptr)
                                    , dataWriteThreadId(nullptr)
                                    , acquisitionThreadId(nullptr)
                                    , acquisitionRunning(false)
{
    errlogPrintf("[%s]: enter...\n", __func__);
    errlogPrintf("portName is %s\n", portName);
    errlogPrintf("numElements is %d\n", numElements);
    errlogPrintf("ipAddress is %s\n", ipAddress);

    // Store IP address
    errlogPrintf("[%s]: store IP address\n", __func__);
    errlogPrintf("size of ipAddress is %ld\n", strlen(ipAddress));
    strncpy(this->ipAddress, ipAddress, strlen(ipAddress));
    this->ipAddress[strlen(ipAddress)] = '\0';

    // Initialize MARS ASIC configuration arrays to zero
    errlogPrintf("[%s]: initialize MARS configuration data\n", __func__);
    memset(globalstr, 0, sizeof(globalstr));
    memset(channelstr, 0, sizeof(channelstr));
    memset(loads, 0, sizeof(loads));

    // Set detector configuration based on number of elements
    switch (numElements)
    {
        case 96:
            nchips = 3;
            break;
        case 192:
            nchips = 6;
            break;
        case 384:
            nchips = 12;
            break;
        default:
            printf("Germanium: Invalid number of elements %d, defaulting to 192\n", numElements);
            this->numElements = 192;
            nchips = 6;
            break;
    }

    printf( "Germanium detector: %d elements, %d chips, IP: %s\n"
          , this->numElements
          , nchips
          , this->ipAddress
          );

    // Allocate dynamic data arrays based on actual number of elements
    errlogPrintf("[%s]: allocating arrays...\n", __func__);
    allocateDataArrays();

    // Create all parameters
    errlogPrintf("[%s]: creating parameters...\n", __func__);
    createGermaniumParameters();

    // Set initial values
    errlogPrintf("[%s]: set initial values\n", __func__);
    setGermaniumInitialValues();

    // Initialize hardware
    initializeGermaniumHardware();

    // Initialize UDP communication
    if (initializeUDPSockets())
    {
        // Create write buffer mutex and event
        writeBufferMutex   = epicsMutexCreate();
        dataWriteAvailable = epicsEventCreate(epicsEventEmpty);
        
        // Start UDP communication threads
        threadsRunning = true;

        udpControlThreadId = epicsThreadCreate( "GermaniumUDPCtrl"
                                              , epicsThreadPriorityMedium
                                              , epicsThreadGetStackSize(epicsThreadStackMedium)
                                              , udpControlThreadC
                                              , this
                                              );

        udpDataThreadId = epicsThreadCreate( "GermaniumUDPData"
                                           , epicsThreadPriorityHigh
                                           , epicsThreadGetStackSize(epicsThreadStackMedium)
                                           , udpDataThreadC
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

        printf("Germanium: UDP communication and data writing threads started\n");
    }
    else
    {
        printf("Germanium: Failed to initialize UDP communication\n");
    }

    // Initialize MARS ASIC configuration
    initializeMarsConfig();

    printf("Germanium detector driver initialized successfully\n");
}

//===========================================================================//

// Destructor
germaniumDetector::~germaniumDetector()
{
    // Stop acquisition and threads
    acquisitionRunning = false;
    threadsRunning = false;
    fileWritingEnabled = false;

    // Wait for threads to finish and destroy them
    if (udpControlThreadId)
    {
        epicsThreadMustJoin(udpControlThreadId);
        udpControlThreadId = nullptr;
    }

    if (udpDataThreadId)
    {
        epicsThreadMustJoin(udpDataThreadId);
        udpDataThreadId = nullptr;
    }

    if (dataProcessingThreadId)
    {
        epicsThreadMustJoin(dataProcessingThreadId);
        dataProcessingThreadId = nullptr;
    }

    if (dataWriteThreadId)
    {
        epicsThreadMustJoin(dataWriteThreadId);
        dataWriteThreadId = nullptr;
    }

    // Close any open data file
    closeCurrentDataFile();
    
    // Close UDP sockets
    closeUDPSockets();

    // Clean up write buffer resources
    if (writeBufferMutex)
    {
        epicsMutexDestroy(writeBufferMutex);
        writeBufferMutex = nullptr;
    }
    
    if (dataWriteAvailable)
    {
        epicsEventDestroy(dataWriteAvailable);
        dataWriteAvailable = nullptr;
    }

    // Clean up UDP resources
    if (udpMutex)
    {
        epicsMutexDestroy(udpMutex);
        udpMutex = nullptr;
    }

    if (dataAvailable)
    {
        epicsEventDestroy(dataAvailable);
        dataAvailable = nullptr;
    }

    printf("Germanium detector driver destroyed\n");
}

//===========================================================================//

/*
 * Member function to create all Germanium detector parameters
 * Based on exact field names and types from original zDDM record
 * All parameter names match zDDMRecord.dbd exactly
 */
void germaniumDetector::createGermaniumParameters()
{
    /* Basic record fields - exact match to zDDM record */
    createParam("VER",     asynParamInt32, &GermaniumVER);      /* Code version */
    createParam("DETTYPE", asynParamInt32, &GermaniumDETTYPE);  /* Detector type */

    /* Large data arrays - exact match to zDDM record */
    createParam("MCA",    asynParamInt32Array,   &GermaniumMCA);    /* MCA spectrum data - NCHAN*4096 */
    createParam("TDC",    asynParamInt32Array,   &GermaniumTDC);    /* TDC spectrum data - NCHAN*1024 */
    createParam("SPCT",   asynParamInt32Array,   &GermaniumSPCT);   /* Selected channel spectrum - 4096 */
    createParam("SPCTX",  asynParamFloat64Array, &GermaniumSPCTX);  /* Calibrated X-axis values - 4096 */
    createParam("INTENS", asynParamInt32Array,   &GermaniumINTENS); /* Intensity array - NELM */

    /* Display size parameters - exact match to zDDM record */
    createParam("EXSIZE", asynParamInt32, &GermaniumEXSIZE); /* Display X size for energy */
    createParam("EYSIZE", asynParamInt32, &GermaniumEYSIZE); /* Display Y size for energy */
    createParam("TXSIZE", asynParamInt32, &GermaniumTXSIZE); /* Display X size for TDC */
    createParam("TYSIZE", asynParamInt32, &GermaniumTYSIZE); /* Display Y size for TDC */

    /* Network configuration - exact match to zDDM record */
    createParam("IPADDR",     asynParamOctet, &GermaniumIPADDR );     /* Fast data IP address */
    createParam("IPADDR_RBV", asynParamOctet, &GermaniumIPADDR_RBV ); /* Fast data IP address */

    /* File handling - exact match to zDDM record */
    createParam("FNAM",  asynParamOctet, &GermaniumFNAM);  /* Filename */
    createParam("CALF",  asynParamOctet, &GermaniumCALF);  /* Calibration filename */
    createParam("DIR",   asynParamOctet, &GermaniumDIR);   /* Data directory path */
    createParam("FSIZE", asynParamInt32, &GermaniumFSIZE); /* Maximum file size in bytes */

    /* Timing and control - exact match to zDDM record */
    createParam("FREQ", asynParamFloat64, &GermaniumFREQ); /* Time base frequency */
    createParam("CNT",  asynParamInt32,   &GermaniumCNT);     /* Count control (menu) */
    createParam("PCNT", asynParamInt32,   &GermaniumPCNT);   /* Previous count (menu) */
    createParam("CONT", asynParamInt32,   &GermaniumCONT);   /* OneShot/AutoCount mode (menu) */
    createParam("MODE", asynParamInt32,   &GermaniumMODE);   /* Timed/Continuous mode (menu) */

    /* Display rates - exact match to zDDM record */
    createParam("RATE", asynParamFloat64, &GermaniumRATE); /* Display rate (Hz) - READ ONLY */
    createParam("RAT1", asynParamFloat64, &GermaniumRAT1); /* Auto display rate (Hz) */

    /* Delays - exact match to zDDM record */
    createParam("DLY",  asynParamFloat64, &GermaniumDLY);   /* Delay */
    createParam("DLY1", asynParamFloat64, &GermaniumDLY1); /* Auto-mode delay */

    /* Time presets - exact match to zDDM record */
    createParam("TP",  asynParamFloat64, &GermaniumTP);   /* Time preset */
    createParam("TP1", asynParamFloat64, &GermaniumTP1);  /* Auto time preset */
    createParam("PR1", asynParamInt32,   &GermaniumPR1);  /* Preset in clock ticks */

    /* State monitoring - exact match to zDDM record */
    createParam("SS", asynParamInt32, &GermaniumSS); /* Scaler state */
    createParam("US", asynParamInt32, &GermaniumUS); /* User state */
    createParam("T", asynParamFloat64, &GermaniumT); /* Timer */

    /* Run control - exact match to zDDM record */
    createParam("RUNNO",     asynParamInt32, &GermaniumRUNNO);      /* Run number */
    createParam("PLDEL",     asynParamInt32, &GermaniumPLDEL);      /* Pipeline delay */
    createParam("PLDEL_RBV", asynParamInt32, &GermaniumPLDEL_RBV);  /* Pipeline delay */
    createParam("RODEL",     asynParamInt32, &GermaniumRODEL );     /* Readout delay */
    createParam("RODEL_RBV", asynParamInt32, &GermaniumRODEL_RBV ); /* Readout delay */

    /* Hardware information - exact match to zDDM record */
    createParam("FVER", asynParamInt32, &GermaniumFVER); /* Firmware version */
    createParam("CARD", asynParamInt32, &GermaniumCARD); /* Card number */

    /* Detector configuration - exact match to zDDM record */
    createParam("NELM", asynParamInt32, &GermaniumNELM);     /* Number of elements */
    createParam("NCH", asynParamInt32, &GermaniumNCH);       /* Number of channels */
    createParam("NCHIPS", asynParamInt32, &GermaniumNCHIPS); /* Number of chips */
    createParam("CHAN", asynParamInt32, &GermaniumCHAN);     /* Channel in chip */
    createParam("CHIP", asynParamInt32, &GermaniumCHIP);     /* Selected chip */

    /* Analog settings - exact match to zDDM record */
    createParam("SHPT", asynParamInt32, &GermaniumSHPT); /* Shaping time (menu) */
    createParam("GAIN", asynParamInt32, &GermaniumGAIN); /* Gain setting (menu) */
    createParam("POL", asynParamInt32, &GermaniumPOL);   /* Input polarity (menu) */
    createParam("EBLK", asynParamInt32, &GermaniumEBLK); /* Enable input bias current (menu) */

    /* Monitor settings - exact match to zDDM record */
    createParam("GMON", asynParamInt32, &GermaniumGMON);   /* Global monitor mode (menu) */
    createParam("MONCH", asynParamInt32, &GermaniumMONCH); /* Monitor channel */
    createParam("LOAO", asynParamInt32, &GermaniumLOAO);   /* Leakage/pulse monitor select (menu) */

    /* Processing settings - exact match to zDDM record */
    createParam("PUEN", asynParamInt32, &GermaniumPUEN); /* Pileup rejection enable (menu) */
    createParam("MFS", asynParamInt32, &GermaniumMFS);   /* Multi-fire suppression (menu) */

    /* TDC settings - exact match to zDDM record */
    createParam("TDS", asynParamInt32, &GermaniumTDS); /* TDC slope (menu) */
    createParam("TDM", asynParamInt32, &GermaniumTDM); /* TDC mode (menu) */

    /* Test pulse settings - exact match to zDDM record */
    createParam( "TPAMP",     asynParamInt32, &GermaniumTPAMP );     /* Test pulse amplitude */
    createParam( "TPAMP_RBV", asynParamInt32, &GermaniumTPAMP_RBV ); /* Test pulse amplitude */
    createParam( "TPFRQ",     asynParamInt32, &GermaniumTPFRQ );     /* Test pulse frequency */
    createParam( "TPFRQ_RBV", asynParamInt32, &GermaniumTPFRQ_RBV ); /* Test pulse frequency */
    createParam( "TPCNT",     asynParamInt32, &GermaniumTPCNT );     /* Number of test pulses */
    createParam( "TPCNT_RBV", asynParamInt32, &GermaniumTPCNT_RBV ); /* Number of test pulses */
    createParam( "TPENB",     asynParamInt32, &GermaniumTPENB );     /* Test pulse enable (menu) */
    createParam( "TPENB_RBV", asynParamInt32, &GermaniumTPENB_RBV ); /* Test pulse enable (menu) */

    /* Per-channel arrays - exact match to zDDM record */
    createParam( "CHEN", asynParamInt8Array, &GermaniumCHEN);    /* Channel enable array */
    createParam( "TSEN", asynParamInt8Array, &GermaniumTSEN);    /* Test pulse input enable array */
    createParam( "THTR", asynParamInt8Array, &GermaniumTHTR);    /* Threshold trim array */
    createParam( "PUTR", asynParamInt8Array, &GermaniumPUTR);    /* Pileup threshold trim array */
    createParam( "SLP",  asynParamFloat64Array, &GermaniumSLP);   /* Slope calibration array */
    createParam( "OFFS", asynParamFloat64Array, &GermaniumOFFS); /* Offset calibration array */

    /* Per-chip arrays - exact match to zDDM record */
    createParam( "THRSH", asynParamInt32Array, &GermaniumTHRSH); /* Threshold array (per chip) */

    /* Acquisition control - exact match to zDDM record */
    createParam( "CLRE", asynParamInt32, &GermaniumCLRE);   /* Clear event spectrum */
    createParam( "CLRM", asynParamInt32, &GermaniumCLRM);   /* Clear monitor spectrum */
    createParam( "CLRT", asynParamInt32, &GermaniumCLRT);   /* Clear timer */
    createParam( "STRT", asynParamInt32, &GermaniumSTRT);   /* Start acquisition */
    createParam( "STOP", asynParamInt32, &GermaniumSTOP);   /* Stop acquisition */

    /* Display and formatting - exact match to zDDM record */
    createParam( "EGU",  asynParamOctet, &GermaniumEGU);   /* Engineering units */
    createParam( "PREC", asynParamInt32, &GermaniumPREC); /* Display precision */

    /* Output links - exact match to zDDM record */
    createParam( "COUT",  asynParamOctet, &GermaniumCOUT);   /* Count output link */
    createParam( "COUTP", asynParamOctet, &GermaniumCOUTP); /* Count output prompt */

    /* Device status */
    createParam( "TEMP1",    asynParamFloat64, &GermaniumTEMP1 );
    createParam( "TEMP2",    asynParamFloat64, &GermaniumTEMP2 );
    createParam( "TEMP3",    asynParamFloat64, &GermaniumTEMP3 );
    createParam( "ZTEMP",    asynParamFloat64, &GermaniumZTEMP );
    createParam( "HV",       asynParamFloat64, &GermaniumHV );
    createParam( "HV_RBV",   asynParamFloat64, &GermaniumHV_RBV );
    createParam( "HV_CURR",  asynParamFloat64, &GermaniumHV_CURR );
}

//===========================================================================//

/*
 * Member function to set initial values for parameters
 * This should be called after createGermaniumParameters() in the constructor
 */
void germaniumDetector::setGermaniumInitialValues()
{
    /* Set default values based on original zDDM record */
    setDoubleParam(GermaniumVER, 0.0);
    setIntegerParam(GermaniumEXSIZE, 4096);
    setIntegerParam(GermaniumEYSIZE, 192);
    setIntegerParam(GermaniumTXSIZE, 1024);
    setIntegerParam(GermaniumTYSIZE, 192);

    setStringParam(GermaniumIPADDR, this->ipAddress);
    setStringParam(GermaniumDIR, "/tmp/germanium/"); /* Default data directory */
    setStringParam(GermaniumFNAM, "germanium_data"); /* Default filename base */
    setIntegerParam(GermaniumFSIZE, 100*1024*1024);  /* Default 100MB file size */
    setDoubleParam(GermaniumFREQ, 1.0e8); /* 1 MHz default */
    setDoubleParam(GermaniumRATE, 2.0);   /* 2 Hz default */
    setDoubleParam(GermaniumTP1, 1.0);    /* 1 second default */

    setIntegerParam(GermaniumNELM, this->numElements);
    setIntegerParam(GermaniumNCHIPS, this->nchips);
    setIntegerParam(GermaniumPLDEL, 72); /* ADC setup and FPGA data alignment */
    setIntegerParam(GermaniumRODEL, 15);

    /* Set menu defaults */
    setIntegerParam(GermaniumCNT, 0);   /* Done */
    setIntegerParam(GermaniumCONT, 0);  /* OneShot */
    setIntegerParam(GermaniumMODE, 0);  /* Framing */
    setIntegerParam(GermaniumSHPT, 1);  /* 0.25us */
    setIntegerParam(GermaniumGAIN, 0);  /* 240keV */
    setIntegerParam(GermaniumPOL, 1);   /* Positive */
    setIntegerParam(GermaniumEBLK, 1);  /* 2pA */
    setIntegerParam(GermaniumGMON, 0);  /* Off */
    setIntegerParam(GermaniumLOAO, 1);  /* Pulse */
    setIntegerParam(GermaniumPUEN, 0);  /* Disable */
    setIntegerParam(GermaniumMFS, 0);   /* Off */
    setIntegerParam(GermaniumTDS, 0);   /* 1us */
    setIntegerParam(GermaniumTDM, 0);   /* Time of arrival */
    setIntegerParam(GermaniumTPENB, 0); /* Off */

    setStringParam(GermaniumEGU, "counts");
    setIntegerParam(GermaniumPREC, 0);

    /* Call callbacks to update all values */
    callParamCallbacks();
}

/*
 * Note: asynPortDriver interface implementations moved to GermaniumDriver.cpp
 * This keeps the main class focused on initialization and core functionality
 */

//===========================================================================//

/*
 * Allocate dynamic data arrays based on numElements using modern C++ containers
 */
void germaniumDetector::allocateDataArrays()
{
    // Resize vectors to appropriate sizes - vectors handle memory automatically
    countRates.resize(numElements, 0); // Initialize all elements to 0
    totalCounts.resize(numElements, 0);

    // Resize 2D vectors
    mcaData.resize(numElements);
    tdcData.resize(numElements);

    // Initialize each element's spectrum arrays
    for (int i = 0; i < numElements; i++)
    {
        mcaData[i].resize(SPECTRUM_SIZE, 0); // Initialize to zero
        tdcData[i].resize(TDC_SIZE, 0);      // Initialize to zero
    }

    // Allocate UDP buffer using smart pointer
    udpDataBuffer = std::make_unique<uint8_t[]>(UDP_BUFFER_SIZE);

    printf("Allocated data arrays for %d detector elements using modern C++ containers\n", numElements);
}

//===========================================================================//

/*
 * Deallocate dynamic data arrays - now mostly automatic with smart pointers/vectors
 */
void germaniumDetector::deallocateDataArrays()
{
    // Vectors automatically clean up their memory when going out of scope
    // But we can explicitly clear them if needed
    mcaData.clear();
    tdcData.clear();
    countRates.clear();
    totalCounts.clear();

    // Smart pointer automatically deallocates when reset or goes out of scope
    udpDataBuffer.reset();

    printf("Deallocated data arrays (automatic with smart pointers)\n");
}

//===========================================================================//

/*
 * Process a single photon event - now using vectors for automatic bounds checking
 */
void germaniumDetector::processPhotonEvent(int element, int energy, int timestamp)
{
    // Bounds checking is automatic with vectors, but we can add explicit checks
    if (element < 0 || element >= numElements)
    {
        printf("Germanium: Invalid element %d (max %d)\n", element, numElements - 1);
        return;
    }

    if (energy < 0 || energy >= SPECTRUM_SIZE)
    {
        printf("Germanium: Invalid energy %d (max %d)\n", energy, SPECTRUM_SIZE - 1);
        return;
    }

    if (timestamp < 0 || timestamp >= TDC_SIZE)
    {
        printf("Germanium: Invalid timestamp %d (max %d)\n", timestamp, TDC_SIZE - 1);
        return;
    }

    // Increment MCA spectrum - vectors provide automatic bounds checking in debug mode
    mcaData[element][energy]++;

    // Increment TDC histogram
    tdcData[element][timestamp]++;

    // Update count statistics
    countRates[element]++;
    totalCounts[element]++;

    // Update global statistics
    evttot++;
}

//===========================================================================//

