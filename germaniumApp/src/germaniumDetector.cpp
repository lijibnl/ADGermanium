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
                                    , nelm_                 ( numElements            )
                                    , mca_nx_               ( 4096                   )
                                    , mca_ny_               ( numElements            )
                                    , tdc_nx_               ( 1024                   )
                                    , tdc_ny_               ( numElements            )
                                    , spct_len_             ( 4096                   )
                                    , intens_len_           ( numElements            )
                                    , mca_addr_             ( 1                      )
                                    , tdc_addr_             ( 2                      )
                                    , spct_addr_            ( 3                      )
                                    , intens_addr_          ( 4                      )
                                    , udpControlSocket      ( -1                     )
                                    , udpDataSocket         ( -1                     )
                                    , udpInitialized        ( false                  )
                                    , zDDMWdTimerQ          ( nullptr                )
                                    , TPgenTimerQ           ( nullptr                )
                                    , numElements           ( numElements            )
                                    , controlPort           ( GERMANIUM_CONTROL_PORT )
                                    , dataPort              ( GERMANIUM_DATA_PORT    )
                                    , udpControlThreadId    ( nullptr                )
                                    , udpDataThreadId       ( nullptr                )
                                    , dataProcessingThreadId( nullptr                )
                                    , threadsRunning        ( false                  )
                                    , udpMutex              ( nullptr                )
                                    , dataAvailable         ( nullptr                )
                                    , udpDataBuffer         ( nullptr                )
                                    , dataBufferSize        ( 0                      )
                                    , evttot                ( 0                      )
                                    , framestat             ( 0                      )
                                    , fileWritingEnabled    ( false                  )
                                    , currentFileHandle     ( -1                     )
                                    , currentFileSize       ( 0                      )
                                    , currentSegmentNumber  ( 0                      )
                                    , totalBytesWritten     ( 0                      )
                                    , totalFilesWritten     ( 0                      )
                                    , writeBufferHead       ( 0                      )
                                    , writeBufferTail       ( 0                      )
                                    , writeBufferCount      ( 0                      )
                                    , writeBufferMutex      ( nullptr                )
                                    , dataWriteAvailable    ( nullptr                )
                                    , dataWriteThreadId     ( nullptr                )
                                    , acquisitionThreadId   ( nullptr                )
                                    , acquisitionRunning    ( false                  )
{
    errlogPrintf("[%s]: enter...\n", __func__);

    // Store IP address
    strncpy(this->ipAddress, ipAddress, strlen(ipAddress));
    this->ipAddress[strlen(ipAddress)] = '\0';

    // Initialize MARS ASIC configuration arrays to zero
    errlogPrintf("[%s]: initialize MARS configuration data\n", __func__);
    memset(globalstr, 0, sizeof(globalstr));
    memset(channelstr, 0, sizeof(channelstr));
    memset(loads, 0, sizeof(loads));

    // Set detector configuration based on number of elements
    int det_type;
    switch (numElements)
    {
        case 96:
            det_type = 0;
            nchips_ = 3;
            break;
        case 192:
            det_type = 0;
            nchips_ = 6;
            break;
        case 384:
            det_type = 1;
            nchips_ = 12;
            break;
        default:
            printf("Germanium: Invalid number of elements %d, defaulting to 192\n", numElements);
            nelm_ = 192;
            nchips_ = 6;
            break;
    }

    printf( "Germanium detector: %d elements, %d chips, IP: %s\n"
          , nelm_
          , nchips_
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

    udpRegisterWrite( DETECTOR_TYPE, det_type );

    // Initialize hardware
    initializeGermaniumHardware();

    // Initialize MARS ASIC configuration
    //initializeMarsConfig();

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
    createParam( GermaniumVersString,    asynParamInt32, &GermaniumVER);      /* Code version */
    createParam( GermaniumDetTypeString, asynParamInt32, &GermaniumDETTYPE);  /* Detector type */

    /* Large data arrays - exact match to zDDM record */
    //createParam( GermaniumMcaString,    asynParamInt32Array,   &GermaniumMCA);    /* MCA spectrum data - NCHAN*4096 */
    //createParam( GermaniumTdcString,    asynParamInt32Array,   &GermaniumTDC);    /* TDC spectrum data - NCHAN*1024 */
    //createParam( GermaniumSpctString,   asynParamInt32Array,   &GermaniumSPCT);   /* Selected channel spectrum - 4096 */
    //createParam( GermaniumSpctxString,  asynParamFloat64Array, &GermaniumSPCTX);  /* Calibrated X-axis values - 4096 */
    //createParam( GermaniumIntensString, asynParamInt32Array,   &GermaniumINTENS); /* Intensity array - NELM */

    /* Display size parameters - exact match to zDDM record */
    createParam( GermaniumExsizeString, asynParamInt32, &GermaniumEXSIZE); /* Display X size for energy */
    createParam( GermaniumEysizeString, asynParamInt32, &GermaniumEYSIZE); /* Display Y size for energy */
    createParam( GermaniumTxsizeString, asynParamInt32, &GermaniumTXSIZE); /* Display X size for TDC */
    createParam( GermaniumTysizeString, asynParamInt32, &GermaniumTYSIZE); /* Display Y size for TDC */

    /* Network configuration - exact match to zDDM record */
    createParam( GermaniumIpaddrString,    asynParamOctet, &GermaniumIPADDR );     /* Fast data IP address */
    createParam( GermaniumIpaddrRbvString, asynParamOctet, &GermaniumIPADDR_RBV ); /* Fast data IP address */

    /* File handling - exact match to zDDM record */
    createParam( GermaniumFnamString,  asynParamOctet, &GermaniumFNAM);  /* Filename */
    createParam( GermaniumCalfString,  asynParamOctet, &GermaniumCALF);  /* Calibration filename */
    createParam( GermaniumDirString,   asynParamOctet, &GermaniumDIR);   /* Data directory path */
    createParam( GermaniumFsizeString, asynParamInt32, &GermaniumFSIZE); /* Maximum file size in bytes */

    /* Timing and control - exact match to zDDM record */
    createParam( GermaniumFreqString, asynParamFloat64, &GermaniumFREQ);   /* Time base frequency */
    createParam( GermaniumCntString,  asynParamInt32,   &GermaniumCNT);    /* Count control (menu) */
    createParam( GermaniumPcntString, asynParamInt32,   &GermaniumPCNT);   /* Previous count (menu) */
    createParam( GermaniumContString, asynParamInt32,   &GermaniumCONT);   /* OneShot/AutoCount mode (menu) */
    createParam( GermaniumModeString, asynParamInt32,   &GermaniumMODE);   /* Timed/Continuous mode (menu) */

    /* Display rates - exact match to zDDM record */
    createParam( GermaniumRateString, asynParamFloat64, &GermaniumRATE); /* Display rate (Hz) - READ ONLY */
    createParam( GermaniumRat1String, asynParamFloat64, &GermaniumRAT1); /* Auto display rate (Hz) */

    /* Delays - exact match to zDDM record */
    createParam( GermaniumDlyString,  asynParamFloat64, &GermaniumDLY);  /* Delay */
    createParam( GermaniumDly1String, asynParamFloat64, &GermaniumDLY1); /* Auto-mode delay */

    /* Time presets - exact match to zDDM record */
    createParam( GermaniumTpString,  asynParamFloat64, &GermaniumTP);   /* Time preset */
    createParam( GermaniumTp1String, asynParamFloat64, &GermaniumTP1);  /* Auto time preset */
    createParam( GermaniumPr1String, asynParamInt32,   &GermaniumPR1);  /* Preset in clock ticks */

    /* State monitoring - exact match to zDDM record */
    createParam( GermaniumSsString, asynParamInt32,  &GermaniumSS); /* Scaler state */
    createParam( GermaniumUsString, asynParamInt32,  &GermaniumUS); /* User state */
    createParam( GermaniumTString, asynParamFloat64, &GermaniumT);  /* Timer */

    /* Run control - exact match to zDDM record */
    createParam( GermaniumRunnoString,     asynParamInt32, &GermaniumRUNNO);      /* Run number */
    createParam( GermaniumPldelString,     asynParamInt32, &GermaniumPLDEL);      /* Pipeline delay */
    createParam( GermaniumPldelRbvString,  asynParamInt32, &GermaniumPLDEL_RBV);  /* Pipeline delay */
    createParam( GermaniumRodelString,     asynParamInt32, &GermaniumRODEL );     /* Readout delay */
    createParam( GermaniumRodelRbvString,  asynParamInt32, &GermaniumRODEL_RBV ); /* Readout delay */

    /* Hardware information - exact match to zDDM record */
    createParam( GermaniumFverString, asynParamInt32, &GermaniumFVER); /* Firmware version */
    createParam( GermaniumCardString, asynParamInt32, &GermaniumCARD); /* Card number */

    /* Detector configuration - exact match to zDDM record */
    createParam( GermaniumNelmString,   asynParamInt32, &GermaniumNELM);     /* Number of elements */
    createParam( GermaniumNchString,    asynParamInt32, &GermaniumNCH);      /* Number of channels */
    createParam( GermaniumNchipsString, asynParamInt32, &GermaniumNCHIPS);   /* Number of chips */
    createParam( GermaniumChanString,   asynParamInt32, &GermaniumCHAN);     /* Channel in chip */
    createParam( GermaniumChipString,   asynParamInt32, &GermaniumCHIP);     /* Selected chip */

    /* Analog settings - exact match to zDDM record */
    createParam( GermaniumShptString, asynParamInt32, &GermaniumSHPT);  /* Shaping time (menu) */
    createParam( GermaniumGainString, asynParamInt32, &GermaniumGAIN);  /* Gain setting (menu) */
    createParam( GermaniumPolString,  asynParamInt32, &GermaniumPOL);   /* Input polarity (menu) */
    createParam( GermaniumEblkString, asynParamInt32, &GermaniumEBLK);  /* Enable input bias current (menu) */

    /* Monitor settings - exact match to zDDM record */
    createParam( GermaniumGmonString,  asynParamInt32, &GermaniumGMON);   /* Global monitor mode (menu) */
    createParam( GermaniumMonchString, asynParamInt32, &GermaniumMONCH);  /* Monitor channel */
    createParam( GermaniumLoaoString,  asynParamInt32, &GermaniumLOAO);   /* Leakage/pulse monitor select (menu) */

    /* Processing settings - exact match to zDDM record */
    createParam( GermaniumPuenString, asynParamInt32, &GermaniumPUEN);  /* Pileup rejection enable (menu) */
    createParam( GermaniumMfsString,  asynParamInt32, &GermaniumMFS);   /* Multi-fire suppression (menu) */

    /* TDC settings - exact match to zDDM record */
    createParam( GermaniumTdsString, asynParamInt32, &GermaniumTDS); /* TDC slope (menu) */
    createParam( GermaniumTdmString, asynParamInt32, &GermaniumTDM); /* TDC mode (menu) */

    /* Test pulse settings - exact match to zDDM record */
    createParam( GermaniumTpampString,     asynParamInt32, &GermaniumTPAMP );     /* Test pulse amplitude */
    //createParam( GermaniumTpampRbvString,  asynParamInt32, &GermaniumTPAMP_RBV ); /* Test pulse amplitude */
    createParam( GermaniumTpfrqString,     asynParamInt32, &GermaniumTPFRQ );     /* Test pulse frequency */
    //createParam( GermaniumTpfrqRBVString,  asynParamInt32, &GermaniumTPFRQ_RBV ); /* Test pulse frequency */
    createParam( GermaniumTpcntString,     asynParamInt32, &GermaniumTPCNT );     /* Number of test pulses */
    //createParam( GermaniumTpcntRbvString,  asynParamInt32, &GermaniumTPCNT_RBV ); /* Number of test pulses */
    createParam( GermaniumTpenbString,     asynParamInt32, &GermaniumTPENB );     /* Test pulse enable (menu) */
    //createParam( GermaniumTpenbRBVString,  asynParamInt32, &GermaniumTPENB_RBV ); /* Test pulse enable (menu) */

    /* Per-channel arrays - exact match to zDDM record */
    createParam( GermaniumChenString,    asynParamInt8Array,    &GermaniumCHEN);    /* Channel enable array */
    createParam( GermaniumTsenString,    asynParamInt8Array,    &GermaniumTSEN);    /* Test pulse input enable array */
    createParam( GermaniumChenSetString, asynParamInt32,        &GermaniumCHEN_SET);/* Set channel enable array */
    createParam( GermaniumTsenSetString, asynParamInt32,        &GermaniumTSEN_SET);/* Set test pulse input enable array */
    createParam( GermaniumThtrString,    asynParamInt8Array,    &GermaniumTHTR);    /* Threshold trim array */
    createParam( GermaniumPutrString,    asynParamInt8Array,    &GermaniumPUTR);    /* Pileup threshold trim array */
    createParam( GermaniumSlpString,     asynParamFloat64Array, &GermaniumSLP);     /* Slope calibration array */
    createParam( GermaniumOffsString,    asynParamFloat64Array, &GermaniumOFFS);    /* Offset calibration array */

    /* Per-chip arrays - exact match to zDDM record */
    createParam( GermaniumThrshString, asynParamInt32Array, &GermaniumTHRSH); /* Threshold array (per chip) */

    /* Acquisition control - exact match to zDDM record */
    //createParam( GermaniumClreString, asynParamInt32, &GermaniumCLRE);   /* Clear event spectrum */
    //createParam( GermaniumClrmString, asynParamInt32, &GermaniumCLRM);   /* Clear monitor spectrum */
    //createParam( GermaniumClrtString, asynParamInt32, &GermaniumCLRT);   /* Clear timer */
    //createParam( GermaniumStrtString, asynParamInt32, &GermaniumSTRT);   /* Start acquisition */
    //createParam( GermaniumStopString, asynParamInt32, &GermaniumSTOP);   /* Stop acquisition */

    /* Display and formatting - exact match to zDDM record */
    createParam( GermaniumEguString,  asynParamOctet, &GermaniumEGU);   /* Engineering units */
    createParam( GermaniumPrecString, asynParamInt32, &GermaniumPREC);  /* Display precision */

    /* Output links - exact match to zDDM record */
    createParam( GermaniumCoutString,  asynParamOctet, &GermaniumCOUT);   /* Count output link */
    createParam( GermaniumCoutpString, asynParamOctet, &GermaniumCOUTP);  /* Count output prompt */

    /* Device status */
    createParam( GermaniumTemp1String,    asynParamFloat64, &GermaniumTEMP1 );
    createParam( GermaniumTemp2String,    asynParamFloat64, &GermaniumTEMP2 );
    createParam( GermaniumTemp3String,    asynParamFloat64, &GermaniumTEMP3 );
    createParam( GermaniumZtempString,    asynParamFloat64, &GermaniumZTEMP );
    createParam( GermaniumHvString,       asynParamFloat64, &GermaniumHV );
    createParam( GermaniumHvRbvString,    asynParamFloat64, &GermaniumHV_RBV );
    createParam( GermaniumHvCurrString,   asynParamFloat64, &GermaniumHV_CURR );
}

//===========================================================================//

/*
 * Member function to set initial values for parameters
 * This should be called after createGermaniumParameters() in the constructor
 */
void germaniumDetector::setGermaniumInitialValues()
{
    /* Set default values based on original zDDM record */
    setDoubleParam(GermaniumVER,     0.0);
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

    setIntegerParam(GermaniumNELM, nelm_ );
    setIntegerParam(GermaniumNCHIPS, nchips_ );
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
 * Allocate dynamic data arrays based on nelm_ using modern C++ containers
 */
void germaniumDetector::allocateDataArrays()
{
    mca_data_.resize( mca_nx_ * mca_ny_ );
    tdc_data_.resize( tdc_nx_ * tdc_ny_ );
    spct_data_.resize( spct_len_ );
    intens_data_.resize( intens_len_ );

    // Resize vectors to appropriate sizes - vectors handle memory automatically
    countRates.resize(nelm_, 0); // Initialize all elements to 0
    totalCounts.resize(nelm_, 0);

    //// Resize 2D vectors
    //mcaData.resize(nelm_);
    //tdcData.resize(nelm_);

    //// Initialize each element's spectrum arrays
    //for (int i = 0; i < nelm_; i++)
    //{
    //    mcaData[i].resize(SPECTRUM_SIZE, 0); // Initialize to zero
    //    tdcData[i].resize(TDC_SIZE, 0);      // Initialize to zero
    //}

    // Allocate UDP buffer using smart pointer
    udpDataBuffer = std::make_unique<uint8_t[]>(UDP_BUFFER_SIZE);
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
    if (element < 0 || element >= nelm_)
    {
        printf("Germanium: Invalid element %d (max %d)\n", element, nelm_ - 1);
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

