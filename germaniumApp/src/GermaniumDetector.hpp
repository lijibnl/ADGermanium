/**
 * @file GermaniumDetector.hpp
 * @brief Class declaration for GermaniumDetector areaDetector driver (ZMQ version).
 *
 * Communicates with Zynq via ZMQ for register-level control (port 5555/5556)
 * and receives raw detector data from PL UDP interface (port 32003).
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * 
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#pragma once

//===========================================================================//

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"

#include "ADDriver.h"
#include "GermaniumDetectorTypes.hpp"
#include "EpicsPoller.hpp"
#include "LockFreeBroadcastSPMC.hpp"
#include "Zmq.hpp"
#include <zmq.h>


//===========================================================================//

/* Parameter string definitions - areaDetector compatible names where possible */

/* Basic info */
#define GermaniumDetModelString      "GERMANIUM_DETMODEL"

/* Data arrays */
#define GermaniumMcaString          "GERMANIUM_MCA"
#define GermaniumTdcString          "GERMANIUM_TDC"
#define GermaniumSpctString         "GERMANIUM_SPCT"
#define GermaniumSpctxString        "GERMANIUM_SPCTX"
#define GermaniumIntensString       "GERMANIUM_INTENS"

/* Display size */
#define GermaniumExsizeString       "GERMANIUM_EXSIZE"
#define GermaniumEysizeString       "GERMANIUM_EYSIZE"
#define GermaniumTxsizeString       "GERMANIUM_TXSIZE"
#define GermaniumTysizeString       "GERMANIUM_TYSIZE"

/* Network */
#define GermaniumIpaddrString       "GERMANIUM_IPADDR"
#define GermaniumIpaddrRbvString    "GERMANIUM_IPADDR_RBV"
#define GermaniumUdpReachableRbvString "GERMANIUM_UDP_REACHABLE_RBV"

/* File handling */
#define GermaniumUdpDataFileWriteEnableString "GERMANIUM_UDP_DATA_FILE_WR_EN"
#define GermaniumFnamString         "GERMANIUM_FNAM"
#define GermaniumCalfString         "GERMANIUM_CALF"
#define GermaniumDirString          "GERMANIUM_DIR"
#define GermaniumFsizeString        "GERMANIUM_FSIZE"

/* Timing and control */
#define GermaniumFreqString         "GERMANIUM_FREQ"
#define GermaniumCntString          "GERMANIUM_CNT"
#define GermaniumCntRbvString       "GERMANIUM_CNT_RBV"
#define GermaniumPcntString         "GERMANIUM_PCNT"
#define GermaniumContString         "GERMANIUM_CONT"
#define GermaniumModeString         "GERMANIUM_MODE"

/* Display rates */
#define GermaniumRateString         "GERMANIUM_RATE"
#define GermaniumRat1String         "GERMANIUM_RAT1"

/* Delays */
#define GermaniumDlyString          "GERMANIUM_DLY"
#define GermaniumDly1String         "GERMANIUM_DLY1"

/* Time presets */
#define GermaniumTpString           "GERMANIUM_TP"
#define GermaniumTp1String          "GERMANIUM_TP1"
#define GermaniumPr1String          "GERMANIUM_PR1"

/* State */
#define GermaniumSsString           "GERMANIUM_SS"
#define GermaniumUsString           "GERMANIUM_US"
#define GermaniumTString            "GERMANIUM_T"

/* Run control */
#define GermaniumRunnoString        "GERMANIUM_RUNNO"
#define GermaniumPldelString        "GERMANIUM_PLDEL"
#define GermaniumPldelRbvString     "GERMANIUM_PLDEL_RBV"
#define GermaniumRodelString        "GERMANIUM_RODEL"
#define GermaniumRodelRbvString     "GERMANIUM_RODEL_RBV"

/* Hardware info */
#define GermaniumFverString         "GERMANIUM_FVER"
#define GermaniumCardString         "GERMANIUM_CARD"

/* Detector config */
#define GermaniumNelmString         "GERMANIUM_NELM"
#define GermaniumNchString          "GERMANIUM_NCH"
#define GermaniumNchipsString       "GERMANIUM_NCHIPS"
#define GermaniumChanString         "GERMANIUM_CHAN"
#define GermaniumChipString         "GERMANIUM_CHIP"

/* Analog settings */
#define GermaniumShptString         "GERMANIUM_SHPT"
#define GermaniumGainString         "GERMANIUM_GAIN"
#define GermaniumPolString          "GERMANIUM_POL"
#define GermaniumEblkString         "GERMANIUM_EBLK"

/* Monitor */
#define GermaniumGmonString         "GERMANIUM_GMON"
#define GermaniumMonchString        "GERMANIUM_MONCH"
#define GermaniumLoaoString         "GERMANIUM_LOAO"

/* Processing */
#define GermaniumPuenString         "GERMANIUM_PUEN"
#define GermaniumMfsString          "GERMANIUM_MFS"

/* TDC */
#define GermaniumTdsString          "GERMANIUM_TDS"
#define GermaniumTdmString          "GERMANIUM_TDM"

/* Test pulse */
#define GermaniumTpampString        "GERMANIUM_TPAMP"
#define GermaniumTpfrqString        "GERMANIUM_TPFRQ"
#define GermaniumTpcntString        "GERMANIUM_TPCNT"
#define GermaniumTpenbString        "GERMANIUM_TPENB"

#define GermaniumTpampRbvString     "GERMANIUM_TPAMP_RBV"
#define GermaniumTpfrqRbvString     "GERMANIUM_TPFRQ_RBV"
#define GermaniumTpcntRbvString     "GERMANIUM_TPCNT_RBV"
#define GermaniumTpenbRbvString     "GERMANIUM_TPENB_RBV"

/* Per-channel arrays */
//#define GermaniumChenString         "GERMANIUM_CHEN"
//#define GermaniumTsenString         "GERMANIUM_TSEN"
#define GermaniumThtrString         "GERMANIUM_THTR"
#define GermaniumPutrString         "GERMANIUM_PUTR"
#define GermaniumSlpString          "GERMANIUM_SLP"
#define GermaniumOffsString         "GERMANIUM_OFFS"

/* Simplified channel enable/test-enable operations */
#define GermaniumChenSelString      "GERMANIUM_CHEN_SEL"
#define GermaniumChenAllString      "GERMANIUM_CHEN_ALL"
#define GermaniumTsenSelString      "GERMANIUM_TSEN_SEL"
#define GermaniumTsenAllString      "GERMANIUM_TSEN_ALL"

/* Thresholds */
#define GermaniumThrshString        "GERMANIUM_THRSH"

/* */
#define GermaniumClreString         "GERMANIUM_CLRE"
#define GermaniumClrmString         "GERMANIUM_CLRM"
#define GermaniumClrtString         "GERMANIUM_CLRT"
#define GermaniumStrtString         "GERMANIUM_STRT"
#define GermaniumStopString         "GERMANIUM_STOP"

/* Display/formatting */
#define GermaniumEguString          "GERMANIUM_EGU"
#define GermaniumPrecString         "GERMANIUM_PREC"

/* Output links */
#define GermaniumCoutString         "GERMANIUM_COUT"
#define GermaniumCoutpString        "GERMANIUM_COUTP"

/* Temperatures */
#define GermaniumTemp1String        "GERMANIUM_TEMP1"
#define GermaniumTemp2String        "GERMANIUM_TEMP2"
#define GermaniumTemp3String        "GERMANIUM_TEMP3"
#define GermaniumZTempString        "GERMANIUM_ZTEMP"

/* High voltage */
#define GermaniumHvString           "GERMANIUM_HV"
#define GermaniumHvRbvString        "GERMANIUM_HV_RBV"
#define GermaniumHvCurrString       "GERMANIUM_HV_CURR"

/* ADCs */
#define GermaniumAdc0ClkSkewString  "GERMANIUM_ADC0_CLK_SKEW"
#define GermaniumAdc1ClkSkewString  "GERMANIUM_ADC1_CLK_SKEW"
#define GermaniumAdc2ClkSkewString  "GERMANIUM_ADC2_CLK_SKEW"

/* MISC status */
#define GermaniumP1String           "GERMANIUM_P1"
#define GermaniumP2String           "GERMANIUM_P2"
#define GermaniumP1CurrString       "GERMANIUM_P1_CURR"
#define GermaniumP2CurrString       "GERMANIUM_P2_CURR"

/* MISC controls */
#define GermaniumLogLevelString     "GERMANIUM_LOG_LEVEL"

//===========================================================================//

class GermaniumDetector : public ADDriver {
public:

    //---------------------------------------------------------------------------//

    GermaniumDetector( const char *portName
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
                     );

    virtual ~GermaniumDetector();

    //---------------------------------------------------------------------------//
    // asynPortDriver overrides
    //---------------------------------------------------------------------------//
    
    virtual asynStatus writeInt32( asynUser *pasynUser, epicsInt32 value );
    virtual asynStatus readInt32( asynUser *pasynUser, epicsInt32 *value );

    virtual asynStatus writeFloat64( asynUser *pasynUser, epicsFloat64 value );
    virtual asynStatus readFloat64( asynUser *pasynUser, epicsFloat64 *value );

    virtual asynStatus writeOctet( asynUser *pasynUser
                                 , const char *value
                                 , size_t maxChars
                                 , size_t *nActual
                                 );
    virtual asynStatus readOctet( asynUser *pasynUser
                                , char *value
                                , size_t maxChars
                                , size_t *nActual
                                , int *eomReason
                                );

    virtual asynStatus readInt32Array( asynUser *pasynUser
                                     , epicsInt32 *value
                                     , size_t nElements
                                     , size_t *nIn
                                     );
    virtual asynStatus writeInt32Array( asynUser *pasynUser
                                      , epicsInt32 *value
                                      , size_t nElements
                                      );

    virtual asynStatus readInt8Array( asynUser *pasynUser
                                    , epicsInt8 *value
                                    , size_t nElements
                                    , size_t *nIn
                                    );
    virtual asynStatus writeInt8Array( asynUser *pasynUser
                                     , epicsInt8 *value
                                     , size_t nElements
                                     );

    virtual asynStatus readFloat64Array( asynUser *pasynUser
                                       , epicsFloat64 *value
                                       , size_t nElements
                                       , size_t *nIn
                                       );                                      
    
    virtual void report(FILE *fp, int details);

    virtual asynStatus drvUserCreate( asynUser *pasynUser
                                    , const char *drvInfo
                                    , const char **pptypeName
                                    , size_t *psize
                                    );

    //---------------------------------------------------------------------------//
    // Parameter creation and initialization
    //---------------------------------------------------------------------------//

    void createGermaniumParameters();
    void setGermaniumInitialValues();

    // Data array management
    void allocateDataArrays();

    //---------------------------------------------------------------------//
    // ZMQ communication — async PUSH-PULL (GermaniumDetectorZmq.cpp)
    //---------------------------------------------------------------------//

    bool initializeZmq();
    //void closeZmq();

    // Non-blocking ZMQ Tx: push to tx queue, return immediately
    asynStatus zmqTx(uint32_t cmd, uint32_t addr, uint32_t value);
    void requestProtocolVersion();

    // Low-level send with logging (called by Tx thread)
    //void zmqSend(const GermaniumProtocol::Message& msg);

    // MARS delta-config helpers (each calls zmqTx)
    asynStatus zmqMarsSetGlobal( uint32_t chipMask
                               , GermaniumProtocol::MarsGlobalField field
                               , uint32_t value
                               );
    asynStatus zmqMarsSetChannel( uint32_t channel
                                , GermaniumProtocol::MarsChannelField field
                                , uint32_t value
                                );
    asynStatus zmqMarsLoad(uint32_t chipMask);

    void zmqTxThread();
    void zmqRxThread();
    //void zmqDataThread();

    static void zmqTxThreadC(void *pPvt);
    static void zmqRxThreadC(void *pPvt);

    // Reply processing (called by Control Rx thread)
    void processReply( const GermaniumProtocol::Message& reply );
    void processReplyProtocolVersion( uint32_t value );
    void processReplyRegRead( uint32_t addr, uint32_t value );
    void processReplyRegWrite( uint32_t addr, uint32_t value );
    void processReplyI2cTempRead( uint32_t addr, uint32_t value );
    void processReplyXadcRead( uint32_t addr, uint32_t value );
    void processReplyI2cAdcRead( uint32_t addr, uint32_t value );
    void processReplyMarsGlobalSet( uint32_t addr, uint32_t value );
    void processReplyMarsGlobalRead( uint32_t addr, uint32_t value );
    void processReplyMarsChannelSet( uint32_t addr, uint32_t value );
    void processReplyMarsChannelRead( uint32_t addr, uint32_t value );
    void processReplyAdcClkSkewSet( uint32_t addr, uint32_t value );
    void processReplyAdcClkSkewRead( uint32_t addr, uint32_t value );
    void processReplyI2cDacWrite( uint32_t addr, uint32_t value );
    void processReplyI2cDacInit( uint32_t addr, uint32_t value );

    //---------------------------------------------------------------------//
    // Read parameters initialized by detector.
    //---------------------------------------------------------------------//
    void readInitParams();

    //---------------------------------------------------------------------//
    // UDP data reception (GermaniumDetectorDataAcq.cpp)
    //---------------------------------------------------------------------//

    bool initializePlUdpSocket();
    void closePlUdpSocket();
    bool initializeUdpRegisterSocket();
    void closeUdpRegisterSocket();
    void plUdpDataThread();
    void dataProcessThread();
    void spectraSynchronizeThread();
    void dataWriteThread();
    void udpWatchdogThread();

   //static void zmqDataThreadC(void *pPvt);
    static void plUdpDataThreadC(void *pPvt);
    static void dataProcessThreadC(void *pPvt);
    static void spectraSynchronizeThreadC(void *pPvt);
    static void dataWriteThreadC(void *pPvt);
    static void udpWatchdogThreadC(void *pPvt);

    // Event processing
    //void processPhotonEvent(int element, int energy, int tdValue);
    void calcSpectra( uint32_t* words, size_t numWords );
    void publishSpectra();
    void clearSpectra();

    //---------------------------------------------------------------------//
    // Acquisition control
    //---------------------------------------------------------------------//

    void startDataAcquisition();
    void stopDataAcquisition();
    void setAcquisitionRunning(bool running);
    void requestUdpReinitialization();

    //---------------------------------------------------------------------//
    
protected:

    //---------------------------------------------------------------------//
    // Parameter indices
    //---------------------------------------------------------------------//

    /* Basic info */
    int GermaniumDETMODEL;

    /* Data arrays */
    int GermaniumMCA, GermaniumTDC, GermaniumSPCT, GermaniumSPCTX, GermaniumINTENS;

    /* Display size */
    int GermaniumEXSIZE, GermaniumEYSIZE, GermaniumTXSIZE, GermaniumTYSIZE;

    /* Network */
    int GermaniumIPADDR, GermaniumIPADDR_RBV, GermaniumUDPReachable_RBV;

    /* File handling */
    int GermaniumUDPDataFileWriteEnable, GermaniumFNAM, GermaniumCALF, GermaniumDIR, GermaniumFSIZE;

    /* Timing and control */
    int GermaniumFREQ, GermaniumCNT, GermaniumCNT_RBV, GermaniumPCNT, GermaniumCONT, GermaniumMODE;

    /* Display rates */
    int GermaniumRATE, GermaniumRAT1;

    /* Delays */
    int GermaniumDLY, GermaniumDLY1;

    /* Time presets */
    int GermaniumTP, GermaniumTP1, GermaniumPR1;

    /* State */
    int GermaniumSS, GermaniumUS, GermaniumT;

    /* Run control */
    int GermaniumRUNNO;
    int GermaniumPLDEL, GermaniumRODEL;
    int GermaniumPLDEL_RBV, GermaniumRODEL_RBV;

    /* Hardware info */
    int GermaniumFVER, GermaniumCARD;

    /* Detector config */
    int GermaniumNELM, GermaniumNCH, GermaniumNCHIPS, GermaniumCHAN, GermaniumCHIP;

    /* Analog settings */
    int GermaniumSHPT, GermaniumGAIN, GermaniumPOL, GermaniumEBLK;

    /* Monitor */
    int GermaniumGMON, GermaniumMONCH, GermaniumLOAO;

    /* Processing */
    int GermaniumPUEN, GermaniumMFS;

    /* TDC */
    int GermaniumTDS, GermaniumTDM;

    /* Test pulse */
    int GermaniumTPAMP, GermaniumTPFRQ, GermaniumTPCNT, GermaniumTPENB;
    int GermaniumTPAMP_RBV, GermaniumTPFRQ_RBV, GermaniumTPCNT_RBV, GermaniumTPENB_RBV;
    int GermaniumCHEN_SEL, GermaniumCHEN_ALL, GermaniumTSEN_SEL, GermaniumTSEN_ALL;

    /* Per-channel arrays */
    //int GermaniumCHEN, GermaniumTSEN
    int GermaniumTHTR, GermaniumPUTR;
    int GermaniumSLP, GermaniumOFFS;
    
    /* Thresholds */
    int GermaniumTHRSH;

    /* */
    int GermaniumCLRE, GermaniumCLRM, GermaniumCLRT, GermaniumSTRT, GermaniumSTOP;

    /* Display/formatting */
    int GermaniumEGU, GermaniumPREC;

    /* Output links */
    int GermaniumCOUT, GermaniumCOUTP;

    /* Temperatures */
    int GermaniumTEMP1, GermaniumTEMP2, GermaniumTEMP3, GermaniumZTEMP;

    /* High voltage */
    int GermaniumHV, GermaniumHV_RBV, GermaniumHV_CURR;
    int GermaniumP1, GermaniumP2, GermaniumP1_CURR, GermaniumP2_CURR;

    /* ADCs */
    int GermaniumADC0_CLK_SKEW, GermaniumADC1_CLK_SKEW, GermaniumADC2_CLK_SKEW;

    /* MISC controls */
    int GermaniumLOG_LEVEL;

    //---------------------------------------------------------------------//

private:

    //---------------------------------------------------------------------//
    // Detector configuration
    //---------------------------------------------------------------------//

    int  numElements;
    char ipAddress[64];
    int  nchips;

    // Clock frequency of the cout timer in FPGA logic
    const float ACQUIRE_TIMER_FREQUENCY = 1.0e6;

    //---------------------------------------------------------------------//
    // Global control and status
    //---------------------------------------------------------------------//

    std::atomic<bool> threadsRunning {true};

    // Acquisition state
    std::atomic<int> evttot {0};

    std::atomic<bool> acquisitionRunning {false};

    //---------------------------------------------------------------------//
    // ZMQ communication
    //---------------------------------------------------------------------//
    zmq::context_t zmqContext{1};
    std::unique_ptr<ZmqClient> zmqClient;
    const std::string zmqTxEndpoint;
    const std::string zmqRxEndpoint;
    std::atomic<bool> zmqNeedReset {false};
    std::atomic<bool> zmqServerDown {false};

    // Async tx queue (EPICS threads → Tx thread → PUSH socket)
    struct TxQueueItem
    {
        GermaniumProtocol::Message msg;
    };
    std::vector<TxQueueItem> txQueue_;
    epicsMutexId txQueueMutex_ {nullptr};
    epicsEventId txQueueEvent_ {nullptr};

    epicsThreadId     zmqTxThreadId {nullptr};
    epicsThreadId     zmqRxThreadId {nullptr};

    //---------------------------------------------------------------------//
    // UDP data related
    //---------------------------------------------------------------------//
    bool udpInit();
    // UDP data socket (raw events from FPGA)
    int  plUdpSocket {-1 };
    bool plUdpInitialized {false};

    // PL UDP register path and watchdog
    int  udpRegisterSocket {-1};
    bool udpRegisterInitialized {false};
    std::atomic<bool> udpInitRequested {true};

    // File handling
    std::atomic<bool>   udpDataFileWriteEnable      {false};
    //std::atomic<bool>   fileWritingEnabled   {false};
    std::atomic<int>    currentFileHandle    {-1};
    std::atomic<size_t> currentFileSize      {0};
    std::atomic<int>    currentSegmentNumber {0};
    std::string         currentFilename;
    std::atomic<size_t> totalBytesWritten    {0};
    std::atomic<int>    totalFilesWritten    {0};
    std::atomic<bool>   flushWriteBuffer     {false};

    epicsThreadId     plUdpDataThreadId          {nullptr};
    epicsThreadId     udpWatchdogThreadId        {nullptr};
    epicsThreadId     dataProcessThreadId        {nullptr};
    epicsThreadId     spectraSynchronizeThreadId {nullptr};
    epicsThreadId     dataWriteThreadId          {nullptr};

    // Lock-free SPMC data queue
    // - Producer: plUdpDataThread
    // - Consumers: dataProcessThread, dataWriteThread
    std::unique_ptr<LockFreeBroadcastSPMC<DataBlock, DATA_QUEUE_CAPACITY, 2>> dataQueue; // heap array [DATA_QUEUE_CAPACITY]

    // The two SPMC consumers wait on events to read data from the queue
    std::array<epicsEvent, 2> udpDataAvailableEvent {epicsEventEmpty, epicsEventEmpty};

    epicsEvent udpWatchdogEvent {epicsEventEmpty};

    // Spectra data — atomic for lock-free access from:
    // - producer: dataProcessThread
    // - consumer: spectraSynchronizeThread
    std::unique_ptr<std::atomic<uint32_t>[]> mcaData;
    std::unique_ptr<std::atomic<uint32_t>[]> tdcData;
    std::unique_ptr<std::atomic<uint32_t>[]> countRates;
    std::unique_ptr<std::atomic<uint64_t>[]> totalCounts;

    int arrayCounter {0};

    void        createDataDirectory();
    std::string generateFilename(int segmentNumber);
    bool        openNewDataFile();
    void        closeCurrentDataFile();
    bool        writeDataToFile(const DataBlock* block);
    void        addDataToBuffer(const uint8_t* data, size_t dataSize);
    //void        flushWriteBuffer();
    bool        getConfiguredUdpAddress(std::string& address);
    void        runUdpInitialization();
    void        runUdpWatchdogProbe();
    bool        udpRegisterWrite(const std::string& targetAddress, uint32_t addr, uint32_t value);
    bool        udpRegisterRead(const std::string& targetAddress, uint32_t addr, uint32_t& value);
    void        setUdpReachable(bool reachable);

    //---------------------------------------------------------------------//
    // Poller related
    //---------------------------------------------------------------------//
    std::unique_ptr<EpicsPoller> poller;

    // Polling info used to configure the poller with op code,
    // register addresses and fast/slow polling rates.
    typedef struct
    {
        uint32_t opCode;
        uint32_t addr;
    }  InitPollInfo;

    typedef struct
    {
        uint32_t opCode;
        uint32_t addr;
        int      slowDivider;
        int      fastDivider;
    }  PollInfo;

    // Any info that needs periodic polling should be added to this array.

    // Poller dividers with 0.1 s base tick: 1 Hz default, 10 Hz fast.
    static constexpr int POLLING_DIVIDER_1_TENTH_HZ  = 100;
    static constexpr int POLLING_DIVIDER_1HZ         = 10;
    static constexpr int POLLING_DIVIDER_10HZ        = 1;
    static constexpr int POLLING_REGULAR = POLLING_DIVIDER_1HZ;
    static constexpr int POLLING_SLOW    = POLLING_DIVIDER_1_TENTH_HZ;
    static constexpr int POLLING_FAST    = POLLING_DIVIDER_10HZ;

    // Poll the parameters initialized by detector once during startup
    static constexpr InitPollInfo initPollInfo[] =
        { { GermaniumProtocol::Command::MARS_GLOBAL_READ, (1u << 16) | GermaniumProtocol::MARS_FIELD_POL   }
        , { GermaniumProtocol::Command::MARS_GLOBAL_READ, (1u << 16) | GermaniumProtocol::MARS_FIELD_GAIN  }
        , { GermaniumProtocol::Command::MARS_GLOBAL_READ, (1u << 16) | GermaniumProtocol::MARS_FIELD_ST    }
        , { GermaniumProtocol::Command::MARS_GLOBAL_READ, (1u << 16) | GermaniumProtocol::MARS_FIELD_TH    }
        , { GermaniumProtocol::Command::MARS_GLOBAL_READ, (1u << 16) | GermaniumProtocol::MARS_FIELD_TPAMP }
        , { GermaniumProtocol::Command::REG_READ,         GermaniumProtocol::Register::VERSIONREG                    }
        , { GermaniumProtocol::Command::REG_READ,         GermaniumProtocol::Register::DETECTOR_MODEL                }
        , { GermaniumProtocol::Command::REG_READ,         GermaniumProtocol::Register::MARS_RDOUT_ENB                }
        , { GermaniumProtocol::Command::ADC_CLK_SKEW_READ, 1                            }
        , { GermaniumProtocol::Command::ADC_CLK_SKEW_READ, 2                            }
        , { GermaniumProtocol::Command::ADC_CLK_SKEW_READ, 3                            }
        };


    static constexpr PollInfo pollInfo[] =
        { { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::TRIG,                POLLING_REGULAR, POLLING_FAST }
        //  { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::CALPULSE_RATE,       POLLING_SLOW, POLLING_FAST    }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::CALPULSE_CNT,        POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::CALPULSE_MODE,       POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::MARS_PIPE_DELAY,     POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::MARS_RDOUT_ENB,      POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::SIM_EVT_SEL,         POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::COUNT_MODE,          POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::EVENT_TIME_CNTR,     POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::COUNT_TIME_LO,       POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::COUNT_TIME_HI,       POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::REG_READ,      GermaniumProtocol::Register::UDP_IP_ADDR,         POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::I2C_TEMP_READ, GermaniumProtocol::TemperatureSelector::TMP100_1, POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::I2C_TEMP_READ, GermaniumProtocol::TemperatureSelector::TMP100_2, POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::I2C_TEMP_READ, GermaniumProtocol::TemperatureSelector::TMP100_3, POLLING_REGULAR, POLLING_FAST }

        //, { GermaniumProtocol::Command::XADC_READ,     0,                                                POLLING_REGULAR, POLLING_FAST }

        //, { GermaniumProtocol::Command::I2C_ADC_READ,  GermaniumProtocol::AdcChannel::HV_VOLTAGE,        POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::I2C_ADC_READ,  GermaniumProtocol::AdcChannel::HV_CURRENT,        POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::I2C_ADC_READ,  GermaniumProtocol::AdcChannel::PELTIER1_CURRENT,  POLLING_REGULAR, POLLING_FAST }
        //, { GermaniumProtocol::Command::I2C_ADC_READ,  GermaniumProtocol::AdcChannel::PELTIER2_CURRENT,  POLLING_REGULAR, POLLING_FAST }

        , { GermaniumProtocol::Command::HEARTBEAT,     0,                                                POLLING_REGULAR, POLLING_REGULAR }
        };
    bool createPoller();

    //---------------------------------------------------------------------//
};

//===========================================================================//
