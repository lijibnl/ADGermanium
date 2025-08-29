/*
 * Germanium.hpp
 * Class declaration for Germanium areaDetector driver
 * Based on Mars_DDM zDDM record implementation
 */

#ifndef GERMANIUM_HPP
#define GERMANIUM_HPP

#include "ADDriver.h"
#include "GermaniumTypes.hpp"
#include "epicsTimer.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pl.h"  // Hardware register definitions from original Mars_DDM

/* Parameter string definitions for Germanium detector fields */
/* Note: Macros used here for EPICS convention and database template compatibility */

/* Basic record fields */
#define GermaniumVersString         "GERMANIUM_VER"        /* Code Version */
#define GermaniumValString          "GERMANIUM_VAL"         /* Value */

// Modern C++ alternative (for future use)
namespace GermaniumParams {
    // Type-safe parameter string constants
    constexpr const char* const Vers = GermaniumVersString;
    constexpr const char* const Val = GermaniumValString;
    // Can add more as needed...
}

#define GermaniumDetTypeString      "GERMANIUM_DETTYPE"     /* Detector type */

/* Data arrays */
#define GermaniumMcaString          "GERMANIUM_MCA"         /* MCA spectrum data */
#define GermaniumTdcString          "GERMANIUM_TDC"         /* TDC spectrum data */
#define GermaniumSpctString         "GERMANIUM_SPCT"        /* Selected channel spectrum */
#define GermaniumSpctxString        "GERMANIUM_SPCTX"       /* Calibrated X-axis values */
#define GermaniumIntensString       "GERMANIUM_INTENS"      /* Intensity array */

/* Display size parameters */
#define GermaniumExsizeString       "GERMANIUM_EXSIZE"      /* Display X size for energy */
#define GermaniumEysizeString       "GERMANIUM_EYSIZE"      /* Display Y size for energy */
#define GermaniumTxsizeString       "GERMANIUM_TXSIZE"      /* Display X size for TDC */
#define GermaniumTysizeString       "GERMANIUM_TYSIZE"      /* Display Y size for TDC */

/* Network configuration */
#define GermaniumIpaddrString       "GERMANIUM_IPADDR"      /* Fast data IP address */
#define GermaniumIpaddrRbvString    "GERMANIUM_IPADDR_RBV"  /* Fast data IP address */

/* File handling */
#define GermaniumFnamString         "GERMANIUM_FNAM"        /* Filename */
#define GermaniumCalfString         "GERMANIUM_CALF"        /* Calibration filename */
#define GermaniumDirString          "GERMANIUM_DIR"         /* Data directory path */
#define GermaniumFsizeString        "GERMANIUM_FSIZE"       /* Maximum file size */

/* Timing and control */
#define GermaniumFreqString         "GERMANIUM_FREQ"        /* Time base frequency */
#define GermaniumCntString          "GERMANIUM_CNT"         /* Count control */
#define GermaniumPcntString         "GERMANIUM_PCNT"        /* Previous count */
#define GermaniumContString         "GERMANIUM_CONT"        /* OneShot/AutoCount mode */
#define GermaniumModeString         "GERMANIUM_MODE"        /* Timed/Continuous mode */

/* Display rates */
#define GermaniumRateString         "GERMANIUM_RATE"        /* Display rate (Hz) */
#define GermaniumRat1String         "GERMANIUM_RAT1"        /* Auto display rate (Hz) */

/* Delays */
#define GermaniumDlyString          "GERMANIUM_DLY"         /* Delay */
#define GermaniumDly1String         "GERMANIUM_DLY1"        /* Auto-mode delay */

/* Time presets */
#define GermaniumTpString           "GERMANIUM_TP"          /* Time preset */
#define GermaniumTp1String          "GERMANIUM_TP1"         /* Auto time preset */
#define GermaniumPr1String          "GERMANIUM_PR1"         /* Preset in clock ticks */

/* State monitoring */
#define GermaniumSsString           "GERMANIUM_SS"          /* Scaler state */
#define GermaniumUsString           "GERMANIUM_US"          /* User state */
#define GermaniumTString            "GERMANIUM_T"           /* Timer */

/* Run control */
#define GermaniumRunnoString        "GERMANIUM_RUNNO"       /* Run number */
#define GermaniumPldelString        "GERMANIUM_PLDEL"       /* Pipeline delay */
#define GermaniumPldelRbvString     "GERMANIUM_PLDEL_RBV"   /* Pipeline delay */
#define GermaniumRodelString        "GERMANIUM_RODEL"       /* Readout delay */
#define GermaniumRodelRbvString     "GERMANIUM_RODEL_RBV"   /* Readout delay */

/* Hardware information */
#define GermaniumFverString         "GERMANIUM_FVER"        /* Firmware version */
#define GermaniumCardString         "GERMANIUM_CARD"        /* Card number */

/* Detector configuration */
#define GermaniumNelmString         "GERMANIUM_NELM"        /* Number of elements */
#define GermaniumNchString          "GERMANIUM_NCH"         /* Number of channels */
#define GermaniumNchipsString       "GERMANIUM_NCHIPS"      /* Number of chips */
#define GermaniumChanString         "GERMANIUM_CHAN"        /* Channel in chip */
#define GermaniumChipString         "GERMANIUM_CHIP"        /* Selected chip */

/* Analog settings */
#define GermaniumShptString         "GERMANIUM_SHPT"        /* Shaping time */
#define GermaniumGainString         "GERMANIUM_GAIN"        /* Gain setting */
#define GermaniumPolString          "GERMANIUM_POL"         /* Input polarity */
#define GermaniumEblkString         "GERMANIUM_EBLK"        /* Enable input bias current */

/* Monitor settings */
#define GermaniumGmonString         "GERMANIUM_GMON"        /* Global monitor mode */
#define GermaniumMonchString        "GERMANIUM_MONCH"       /* Monitor channel */
#define GermaniumLoaoString         "GERMANIUM_LOAO"        /* Leakage/pulse monitor select */

/* Processing settings */
#define GermaniumPuenString         "GERMANIUM_PUEN"        /* Pileup rejection enable */
#define GermaniumMfsString          "GERMANIUM_MFS"         /* Multi-fire suppression */

/* TDC settings */
#define GermaniumTdsString          "GERMANIUM_TDS"         /* TDC slope */
#define GermaniumTdmString          "GERMANIUM_TDM"         /* TDC mode */

/* Test pulse settings */
#define GermaniumTpampString        "GERMANIUM_TPAMP"       /* Test pulse amplitude */
#define GermaniumTpfrqString        "GERMANIUM_TPFRQ"       /* Test pulse frequency */
#define GermaniumTpcntString        "GERMANIUM_TPCNT"       /* Number of test pulses */
#define GermaniumTpenbString        "GERMANIUM_TPENB"       /* Test pulse enable */

/* Per-channel arrays */
#define GermaniumChenString         "GERMANIUM_CHEN"        /* Channel enable array */
#define GermaniumTsenString         "GERMANIUM_TSEN"        /* Test pulse input enable array */
#define GermaniumThtrString         "GERMANIUM_THTR"        /* Threshold trim array */
#define GermaniumPutrString         "GERMANIUM_PUTR"        /* Pileup threshold trim array */
#define GermaniumSlpString          "GERMANIUM_SLP"         /* Slope calibration array */
#define GermaniumOffsString         "GERMANIUM_OFFS"        /* Offset calibration array */

/* Per-chip arrays */
#define GermaniumThrshString        "GERMANIUM_THRSH"       /* Threshold array (per chip) */

/* Display and formatting */
#define GermaniumEguString          "GERMANIUM_EGU"         /* Engineering units */
#define GermaniumPrecString         "GERMANIUM_PREC"        /* Display precision */

/* Output links */
#define GermaniumCoutString         "GERMANIUM_COUT"        /* Count output link */
#define GermaniumCoutpString        "GERMANIUM_COUTP"       /* Count output prompt */

/* Device status */
#define GermaniumTemp1String        "GERMATNIUM_TEMP1"
#define GermaniumTemp2String        "GERMATNIUM_TEMP2"
#define GermaniumTemp3String        "GERMATNIUM_TEMP3"
#define GermaniumZTempString        "GERMATNIUM_ZTEMP"
#define GermaniumHvString           "GERMATNIUM_HV"
#define GermaniumHvRbvString        "GERMATNIUM_HV_RBV"
#define GermaniumHvCurrString       "GERMATNIUM_HV_CURR"

class Germanium : public ADDriver {
public:
    // Constructor for photon counting Germanium detector
    // maxAddr should equal numElements (one address per detector element)
    // numParams is the total number of parameters (calculated from createParam calls)
    // maxBuffers can be small (10-20) since data accumulates in histograms
    // maxMemory depends on spectrum size: numElements × spectrumSize × sizeof(data)
    Germanium(const char *portName, int numElements, const char *ipAddress,
              int maxAddr, int numParams, int maxBuffers, size_t maxMemory,
              int interfaceMask, int interruptMask,
              int asynFlags, int autoConnect, int priority, int stackSize);
    
    // Destructor
    virtual ~Germanium();
    
    // asynPortDriver virtual methods - overridden for UDP communication
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars,
                                  size_t *nActual);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                      size_t nElements, size_t *nIn);
    virtual asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                       size_t nElements);

    // ADDriver virtual methods for image acquisition
    virtual asynStatus readNDArray(asynUser *pasynUser, epicsInt32 *value,
                                   size_t nElements, size_t *nIn);
    virtual void report(FILE *fp, int details);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);

    // Parameter creation and initialization
    void createGermaniumParameters();
    void setGermaniumInitialValues();
    
    // Photon counting specific methods
    void processPhotonEvent(int element, int energy, int time);
    void updateSpectra();
    void clearSpectra();
    void updateCountRates();
    
    // Dynamic array management
    void allocateDataArrays();
    void deallocateDataArrays();
    
    // UDP communication methods (implemented in GermaniumNetwork.cpp)
    int make_udp_bind(int port);
    void set_nonblock( int s );
    bool initializeUDPSockets();
    void closeUDPSockets();
    asynStatus sendUDPCommand( uint16_t op, uint32_t data);
    void udpControlThread();      // Thread for control/status UDP reception
    void udpDataThread();         // Thread for data UDP reception  
    void dataProcessingThread();  // Thread for processing received data
    void dataWriteThread();       // Thread for writing data to files
    
    // Static thread entry points (implemented in GermaniumDataAcq.cpp and GermaniumHardware.cpp)
    static void udpControlThreadC(void *pPvt);
    static void udpDataThreadC(void *pPvt);
    static void dataProcessingThreadC(void *pPvt);
    static void dataWriteThreadC(void *pPvt);
    
    // UDP-based hardware interface (implemented in GermaniumNetwork.cpp)
    asynStatus udpRegisterWrite(uint32_t reg, uint32_t value);
    asynStatus udpRegisterRead(uint32_t reg);
    //asynStatus udpSendString(uint32_t command, const char *str);
    asynStatus sendMarsConfiguration();  // Send entire loads[] array via UDP
    asynStatus udpWriteIntArray(uint32_t command, const void *data, 
                                size_t dataSize, uint32_t address);
    asynStatus udpSendLoads( uint32_t* loads, size_t count );
    void fifo_reset();                // Now sends UDP command
    void fifo_disable();              // Now sends UDP command  
    void ad9252_cnfg(int adc, int reg, int value); // Now sends UDP command

protected:
    // Parameter indices - these will be defined based on createParam() calls
    int GermaniumVER, GermaniumVAL, GermaniumDETTYPE;
    int GermaniumMCA, GermaniumTDC, GermaniumSPCT, GermaniumSPCTX, GermaniumINTENS;
    int GermaniumEXSIZE, GermaniumEYSIZE, GermaniumTXSIZE, GermaniumTYSIZE;
    int GermaniumIPADDR, GermaniumIPADDR_RBV;
    int GermaniumFNAM, GermaniumCALF, GermaniumDIR, GermaniumFSIZE;
    int GermaniumFREQ, GermaniumCNT, GermaniumPCNT, GermaniumCONT, GermaniumMODE;
    int GermaniumRATE, GermaniumRAT1, GermaniumDLY, GermaniumDLY1;
    int GermaniumTP, GermaniumTP1, GermaniumPR1;
    int GermaniumSS, GermaniumUS, GermaniumT;
    int GermaniumRUNNO;
    int GermaniumPLDEL, GermaniumRODEL;
    int GermaniumPLDEL_RBV, GermaniumRODEL_RBV;
    int GermaniumFVER, GermaniumCARD;
    int GermaniumNELM, GermaniumNCH, GermaniumNCHIPS, GermaniumCHAN, GermaniumCHIP;
    int GermaniumSHPT, GermaniumGAIN, GermaniumPOL, GermaniumEBLK;
    int GermaniumGMON, GermaniumMONCH, GermaniumLOAO;
    int GermaniumPUEN, GermaniumMFS;
    int GermaniumTDS, GermaniumTDM;
    int GermaniumTPAMP, GermaniumTPFRQ, GermaniumTPCNT, GermaniumTPENB;
    int GermaniumTPAMP_RBV, GermaniumTPFRQ_RBV, GermaniumTPCNT_RBV, GermaniumTPENB_RBV;
    int GermaniumCHEN, GermaniumTSEN, GermaniumTHTR, GermaniumPUTR;
    int GermaniumSLP, GermaniumOFFS, GermaniumTHRSH;
    int GermaniumEGU, GermaniumPREC;
    int GermaniumCOUT, GermaniumCOUTP;
    int GermaniumCLRE, GermaniumCLRM, GermaniumCLRT, GermaniumSTRT, GermaniumSTOP;
    int GermaniumTEMP1, GermaniumTEMP2, GermaniumTEMP3, GermaniumZTEMP;
    int GermaniumHV, GermaniumHV_RBV, GermaniumHV_CURR;

private:
    // Data acquisition and file management
    void startDataAcquisition();
    void stopDataAcquisition();
    void createDataDirectory();
    std::string generateFilename(int segmentNumber);
    bool openNewDataFile();
    void closeCurrentDataFile();
    bool writeDataToFile(const uint8_t* data, size_t dataSize);
    void addDataToWriteBuffer(const uint8_t* data, size_t dataSize);
    void flushWriteBuffer();
    
    // Data processing methods
    void processResponse( const uint8_t* data, size_t dataSize );
    void processReceivedData(const uint8_t* data, size_t dataSize);
    void processSpectrumData(const uint8_t* data, size_t dataSize);
    void processEventData(const uint8_t* data, size_t dataSize);
    void processStatusDat(const uint8_t* data, size_t dataSize);
    
    // Hardware-related methods (now UDP-based instead of direct FIFO access)
    void initializeGermaniumHardware();
    void initializeMarsConfig();
    void setupDataAcquisition();
    //asynStatus udpSendMarsGlobal(int chip, MarsGlobalConfig *config);
    //asynStatus udpSendMarsChannels();
    
    // 
    // MARS ASIC configuration optimization methods
    template<typename T>
    constexpr uint32_t packBits(T value, int position, int width) {
        return (static_cast<uint32_t>(value) & ((1U << width) - 1)) << position;
    }
    
    uint32_t packGlobalConfig(const globalstr_t& global);
    uint16_t packChannelConfig(const channelstr_t& channel);
    void wrapOptimized();
    void wrapBitFields();
    void validateConfiguration();
    
    // For compatibility with original Mars_DDM (now sends UDP commands)
    void wrap() { wrapOptimized(); }
    
    // MARS configuration management
    void updateLoadsArray();          // Update loads[] from globalstr/channelstr
    void sendConfigurationToDevice(); // Send complete configuration via UDP
    
    // UDP communication state variables (replacing direct device access)
    int udpControlSocket;             // Socket for control commands
    int udpDataSocket;                // Socket for data reception  
    struct sockaddr_in deviceAddr;   // Device UDP address
    bool udpInitialized;              // UDP initialization status
    epicsTimerQueueId zDDMWdTimerQ;   // Watchdog timer queue
    epicsTimerQueueId TPgenTimerQ;    // Test pulse timer queue
    
    // Detector configuration
    int numElements;                  // Number of detector elements (96/192/384)
    char ipAddress[32];               // IP address for detector communication
    uint16_t controlPort;             // UDP port for control commands 
    uint16_t dataPort;                // UDP port for data reception 
    
    // Thread management for UDP communication
    epicsThreadId udpControlThreadId; // Control/status UDP receiver thread
    epicsThreadId udpDataThreadId;    // Data UDP receiver thread
    epicsThreadId dataProcessingThreadId; // Data processing thread
    bool threadsRunning;              // Flag to control thread execution
    epicsMutexId udpMutex;            // Mutex for UDP socket access
    epicsEventId dataAvailable;       // Event for data processing synchronization
    
    // Data buffers for UDP reception
    std::unique_ptr<uint8_t[]> udpDataBuffer;     // Buffer for incoming data packets (smart pointer)
    size_t dataBufferSize;                        // Current data in buffer
    
    // MARS ASIC configuration arrays - stack-based for simplicity
    static constexpr int MAX_CHIPS = 12;
    static constexpr int MAX_CHANNELS = 384;
    static constexpr int MAX_LOADS = 2048;
    
    globalstr_t globalstr[MAX_CHIPS];      // Global settings per chip
    channelstr_t channelstr[MAX_CHANNELS]; // Per-channel settings  
    uint32_t loads[12][14];                // SPI configuration data
    int nchips;                            // Number of chips actually used
    
    // Data acquisition state
    int evttot;                       // Total events processed
    int framestat;                    // Frame status
    
    // File handling state
    bool fileWritingEnabled;          // Whether file writing is active
    int currentFileHandle;            // Current open file descriptor
    size_t currentFileSize;           // Current file size in bytes
    int currentSegmentNumber;         // Current segment number
    std::string currentFilename;      // Current filename
    size_t totalBytesWritten;         // Total bytes written across all files
    int totalFilesWritten;            // Total number of files written
    
    // Write buffer for data writing thread
    std::vector<uint8_t> dataWriteBuffer;  // Circular buffer for file writing
    size_t writeBufferHead;               // Write position in buffer
    size_t writeBufferTail;               // Read position in buffer
    size_t writeBufferCount;              // Number of bytes in buffer
    epicsMutexId writeBufferMutex;        // Mutex for buffer access
    epicsEventId dataWriteAvailable;      // Event for data writing thread
    epicsThreadId dataWriteThreadId;      // Data writing thread
    
    // Photon counting data arrays - using modern C++ containers for better memory management
    std::vector<std::vector<uint32_t>> mcaData;   // MCA spectra per element [numElements][SPECTRUM_SIZE]
    std::vector<std::vector<uint32_t>> tdcData;   // TDC histograms per element [numElements][TDC_SIZE]
    std::vector<uint32_t> countRates;             // Current count rate per element [numElements]
    std::vector<uint64_t> totalCounts;            // Total counts per element [numElements]
    
    // Thread management
    epicsThreadId acquisitionThreadId;
    bool acquisitionRunning;
    
    // Data packet structure for photon events
    struct PhotonEvent {
        uint16_t element;             // Detector element (0-383)
        uint16_t energy;              // Energy (ADC counts)
        uint32_t timestamp;           // Event timestamp
    } __attribute__((packed));
    
    // Static callback functions for C compatibility (no longer used for direct access)
    static void frame_done(int sig);
    static void event_publish(void *arg);
};

// Bit-field structures for optimized MARS configuration
struct __attribute__((packed)) MarsGlobalConfigBits {
    uint32_t pa     : 10;   // Threshold DAC
    uint32_t pb     : 8;    // Test pulse DAC
    uint32_t rm     : 1;    // Readout mode
    uint32_t senfl1 : 1;    // Lock on peak found
    uint32_t senfl2 : 1;    // Lock on threshold
    uint32_t m0     : 1;    // Monitor mode
    uint32_t m1     : 1;    // Peak detector mode
    uint32_t sbn    : 1;    // Enable buffer
    uint32_t sb     : 1;    // Enable buffer
    uint32_t sl     : 2;    // Leakage current
    uint32_t ts     : 2;    // Shaping time
    uint32_t rt     : 3;    // Timing ramp
};

struct __attribute__((packed)) MarsChannelConfigBits {
    uint16_t dp     : 4;    // Pileup trim DAC
    uint16_t da     : 4;    // Threshold trim DAC
    uint16_t sel    : 1;    // Monitor select
    uint16_t sm     : 1;    // Channel enable
    uint16_t st     : 1;    // Test input
    uint16_t unused : 5;    // Padding
};

#endif // GERMANIUM_HPP
