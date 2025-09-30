/**
 * @file germaniumDetector.hpp
 * @brief Class declaration for germaniumDetector areaDetector driver.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */
#pragma once

//===========================================================================//

#include "ADDriver.h"
#include "germaniumDetectorTypes.hpp"
#include "epicsTimer.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <span>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "germaniumDetectorRegister.hpp"  // Hardware register definitions from original Mars_DDM

//===========================================================================//

//===========================================================================//

class germaniumDetector : public ADDriver {
public:
    // Constructor for photon counting Germanium detector
    // maxAddr should equal numElements (one address per detector element)
    // numParams is the total number of parameters (calculated from createParam calls)
    // maxBuffers can be small (10-20) since data accumulates in histograms
    // maxMemory depends on spectrum size: numElements × spectrumSize × sizeof(data)
    germaniumDetector(const char *portName, int numElements, const char *ipAddress,
              int maxAddr, int numParams, int maxBuffers, size_t maxMemory,
              int mcaAddr, int tdcAddr, int spctAddr, int intensAddr,
              int interfaceMask, int interruptMask,
              int asynFlags, int autoConnect, int priority, int stackSize);
    
    // Destructor
    ~germaniumDetector();
    
    // asynPortDriver virtual methods - overridden for UDP communication
    asynStatus writeInt32( asynUser *pasynUser, epicsInt32 value ) override;
    asynStatus readInt32( asynUser *pasynUser, epicsInt32* value ) override;
    asynStatus writeFloat64( asynUser *pasynUser, epicsFloat64 value ) override;
    
    asynStatus writeOctet( asynUser *pasynUser
                         , const char *value
                         , size_t maxChars
                         , size_t *nActual
                         ) override;
    
    asynStatus readInt32Array( asynUser *pasynUser
                             , epicsInt32 *value
                             , size_t nElements
                             , size_t *nIn
                             ) override;

    asynStatus writeInt32Array( asynUser *pasynUser
                              , epicsInt32 *value
                              , size_t nElements
                              ) override;

    //// ADDriver virtual methods for image acquisition
    //asynStatus readNDArray( asynUser *pasynUser
    //                      , epicsInt32 *value
    //                      , size_t nElements
    //                      , size_t *nIn
    //                      ) override;

    void report(FILE *fp, int details) override;
    asynStatus drvUserCreate( asynUser *pasynUser
                            , const char *drvInfo
                            , const char **pptypeName
                            , size_t *psize
                            ) override;

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
    asynStatus ad9252_cnfg(int adc, int value); // Now sends UDP command

protected:
    // Parameter indices - these will be defined based on createParam() calls
    int GermaniumVER, GermaniumVAL, GermaniumDETTYPE;
    int GermaniumMCA, GermaniumTDC, GermaniumSPCT, GermaniumSPCTX, GermaniumINTENS;
    int GermaniumEXSIZE, GermaniumEYSIZE, GermaniumTXSIZE, GermaniumTYSIZE;
    int GermaniumIPADDR, GermaniumIPADDR_RBV;
    int GermaniumFNAM, GermaniumCALF, GermaniumDIR, GermaniumFSIZE;
    int GermaniumFREQ, GermaniumCNT, GermaniumCNT_RBV, GermaniumPCNT, GermaniumCONT, GermaniumMODE;
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
    int GermaniumCHEN, GermaniumTSEN, GermaniumCHEN_SET, GermaniumTSEN_SET;
    int GermaniumTHTR, GermaniumPUTR;
    int GermaniumSLP, GermaniumOFFS, GermaniumTHRSH;
    int GermaniumEGU, GermaniumPREC;
    int GermaniumADC0_SKEW, GermaniumADC1_SKEW, GermaniumADC2_SKEW;
    int GermaniumCOUT, GermaniumCOUTP;
    int GermaniumCLRE, GermaniumCLRM, GermaniumCLRT, GermaniumSTRT, GermaniumSTOP;
    int GermaniumTEMP1, GermaniumTEMP2, GermaniumTEMP3, GermaniumZTEMP;
    int GermaniumHV, GermaniumHV_RBV, GermaniumHV_CURR;


    // Parameter string definitions - converted from macros to static constexpr std::string
    static constexpr std::string GermaniumVersString {"VER"};
    static constexpr std::string GermaniumValString {"VAL"};
    static constexpr std::string GermaniumDetTypeString {"DETTYPE"};
    static constexpr std::string GermaniumMcaString {"MCA"};
    static constexpr std::string GermaniumTdcString {"TDC"};
    static constexpr std::string GermaniumSpctString {"SPCT"};
    static constexpr std::string GermaniumSpctxString {"SPCTX"};
    static constexpr std::string GermaniumIntensString {"INTENS"};
    static constexpr std::string GermaniumExsizeString {"EXSIZE"};
    static constexpr std::string GermaniumEysizeString {"EYSIZE"};
    static constexpr std::string GermaniumTxsizeString {"TXSIZE"};
    static constexpr std::string GermaniumTysizeString {"TYSIZE"};
    static constexpr std::string GermaniumIpaddrString {"IPADDR"};
    static constexpr std::string GermaniumIpaddrRbvString {"IPADDR_RBV"};
    static constexpr std::string GermaniumFnamString {"FNAM"};
    static constexpr std::string GermaniumCalfString {"CALF"};
    static constexpr std::string GermaniumDirString {"DIR"};
    static constexpr std::string GermaniumFsizeString {"FSIZE"};
    static constexpr std::string GermaniumFreqString {"FREQ"};
    static constexpr std::string GermaniumCntString {"CNT"};
    static constexpr std::string GermaniumCntRbvString {"CNT_RBV"};
    static constexpr std::string GermaniumPcntString {"PCNT"};
    static constexpr std::string GermaniumContString {"CONT"};
    static constexpr std::string GermaniumModeString {"MODE"};
    static constexpr std::string GermaniumRateString {"RATE"};
    static constexpr std::string GermaniumRat1String {"RAT1"};
    static constexpr std::string GermaniumDlyString {"DLY"};
    static constexpr std::string GermaniumDly1String {"DLY1"};
    static constexpr std::string GermaniumTpString {"TP"};
    static constexpr std::string GermaniumTp1String {"TP1"};
    static constexpr std::string GermaniumPr1String {"PR1"};
    static constexpr std::string GermaniumSsString {"SS"};
    static constexpr std::string GermaniumUsString {"US"};
    static constexpr std::string GermaniumTString {"T"};
    static constexpr std::string GermaniumRunnoString {"RUNNO"};
    static constexpr std::string GermaniumPldelString {"PLDEL"};
    static constexpr std::string GermaniumPldelRbvString {"PLDEL_RBV"};
    static constexpr std::string GermaniumRodelString {"RODEL"};
    static constexpr std::string GermaniumRodelRbvString {"RODEL_RBV"};
    static constexpr std::string GermaniumFverString {"FVER"};
    static constexpr std::string GermaniumCardString {"CARD"};
    static constexpr std::string GermaniumNelmString {"NELM"};
    static constexpr std::string GermaniumNchString {"NCH"};
    static constexpr std::string GermaniumNchipsString {"NCHIPS"};
    static constexpr std::string GermaniumChanString {"CHAN"};
    static constexpr std::string GermaniumChipString {"CHIP"};
    static constexpr std::string GermaniumShptString {"SHPT"};
    static constexpr std::string GermaniumGainString {"GAIN"};
    static constexpr std::string GermaniumPolString {"POL"};
    static constexpr std::string GermaniumEblkString {"EBLK"};
    static constexpr std::string GermaniumGmonString {"GMON"};
    static constexpr std::string GermaniumMonchString {"MONCH"};
    static constexpr std::string GermaniumLoaoString {"LOAO"};
    static constexpr std::string GermaniumPuenString {"PUEN"};
    static constexpr std::string GermaniumMfsString {"MFS"};
    static constexpr std::string GermaniumTdsString {"TDS"};
    static constexpr std::string GermaniumTdmString {"TDM"};
    static constexpr std::string GermaniumTpampString {"TPAMP"};
    static constexpr std::string GermaniumTpfrqString {"TPFRQ"};
    static constexpr std::string GermaniumTpcntString {"TPCNT"};
    static constexpr std::string GermaniumTpenbString {"TPENB"};
    static constexpr std::string GermaniumChenString {"CHEN"};
    static constexpr std::string GermaniumTsenString {"TSEN"};
    static constexpr std::string GermaniumChenSetString {"CHEN_SET"};
    static constexpr std::string GermaniumTsenSetString {"TSEN_SET"};
    static constexpr std::string GermaniumThtrString {"THTR"};
    static constexpr std::string GermaniumPutrString {"PUTR"};
    static constexpr std::string GermaniumSlpString {"SLP"};
    static constexpr std::string GermaniumOffsString {"OFFS"};
    static constexpr std::string GermaniumThrshString {"THRSH"};
    static constexpr std::string GermaniumEguString {"EGU"};
    static constexpr std::string GermaniumPrecString {"PREC"};
    static constexpr std::string GermaniumAdc0SkewString {"ADC0_SKEW"};
    static constexpr std::string GermaniumAdc1SkewString {"ADC1_SKEW"};
    static constexpr std::string GermaniumAdc2SkewString {"ADC2_SKEW"};
    static constexpr std::string GermaniumCoutString {"COUT"};
    static constexpr std::string GermaniumCoutpString {"COUTP"};
    static constexpr std::string GermaniumTemp1String {"TEMP1"};
    static constexpr std::string GermaniumTemp2String {"TEMP2"};
    static constexpr std::string GermaniumTemp3String {"TEMP3"};
    static constexpr std::string GermaniumZtempString {"ZTEMP"};
    static constexpr std::string GermaniumHvString {"HV"};
    static constexpr std::string GermaniumHvRbvString {"HV_RBV"};
    static constexpr std::string GermaniumHvCurrString {"HV_CURR"};

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
    
    void publish2DUInt32Array( const std::vector<uint32_t>& vec 
                             , size_t nx
                             , size_t ny
                             , int addr
                             );

    void publish1DUInt32Array( std::span<const uint32_t> vec
                             , int addr
                             );

    void publishMCA();
    void publishTDC();
    void publishSPCT();
    void publishINTENS();

    // Hardware-related methods (now UDP-based instead of direct FIFO access)
    void wrap();
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

    void publishData();

    // For detector type
    int nelm_, nchips_;

    // Arrays
    const size_t mca_nx_, mca_ny_;
    const size_t tdc_nx_, tdc_ny_;
    const size_t spct_len_;
    const size_t intens_len_;

    const int mca_addr_, tdc_addr_, spct_addr_, intens_addr_;

    std::vector<uint32_t> mca_data_;
    std::vector<uint32_t> tdc_data_;
    std::vector<uint32_t> spct_data_;
    std::vector<uint32_t> intens_data_;

    // For test pulses
    epicsInt8 tsen_[384], chen_[384];
    epicsInt32 thrsh_[12];
    
    // For compatibility with original Mars_DDM (now sends UDP commands)
    //void wrap() { wrapOptimized(); }
    
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
    //int nchips;                            // Number of chips actually used
    
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

//===========================================================================//

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

//===========================================================================//

struct __attribute__((packed)) MarsChannelConfigBits {
    uint16_t dp     : 4;    // Pileup trim DAC
    uint16_t da     : 4;    // Threshold trim DAC
    uint16_t sel    : 1;    // Monitor select
    uint16_t sm     : 1;    // Channel enable
    uint16_t st     : 1;    // Test input
    uint16_t unused : 5;    // Padding
};

//===========================================================================//

