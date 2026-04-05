/**
 * @file germaniumDetector.hpp
 * @brief Class declaration for germaniumDetector areaDetector driver (ZMQ version).
 *
 * Communicates with Zynq via ZMQ for register-level control (port 5555/5556)
 * and receives raw detector data from PL UDP interface (port 32003).
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
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include <zmq.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "germaniumDetectorRegister.hpp"

//===========================================================================//

/* Parameter string definitions - areaDetector compatible names where possible */

/* Basic info */
#define GermaniumVersString         "GERMANIUM_VER"
#define GermaniumDetTypeString      "GERMANIUM_DETTYPE"

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

/* File handling */
#define GermaniumFnamString         "GERMANIUM_FNAM"
#define GermaniumCalfString         "GERMANIUM_CALF"
#define GermaniumDirString          "GERMANIUM_DIR"
#define GermaniumFsizeString        "GERMANIUM_FSIZE"

/* Timing and control */
#define GermaniumFreqString         "GERMANIUM_FREQ"
#define GermaniumCntString          "GERMANIUM_CNT"
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

/* Per-channel arrays */
#define GermaniumChenString         "GERMANIUM_CHEN"
#define GermaniumTsenString         "GERMANIUM_TSEN"
#define GermaniumThtrString         "GERMANIUM_THTR"
#define GermaniumPutrString         "GERMANIUM_PUTR"
#define GermaniumSlpString          "GERMANIUM_SLP"
#define GermaniumOffsString         "GERMANIUM_OFFS"

/* Per-chip arrays */
#define GermaniumThrshString        "GERMANIUM_THRSH"

/* Display/formatting */
#define GermaniumEguString          "GERMANIUM_EGU"
#define GermaniumPrecString         "GERMANIUM_PREC"

/* Output links */
#define GermaniumCoutString         "GERMANIUM_COUT"
#define GermaniumCoutpString        "GERMANIUM_COUTP"

/* Device status (I2C sensors — not available via current ZMQ server) */
#define GermaniumTemp1String        "GERMANIUM_TEMP1"
#define GermaniumTemp2String        "GERMANIUM_TEMP2"
#define GermaniumTemp3String        "GERMANIUM_TEMP3"
#define GermaniumZTempString        "GERMANIUM_ZTEMP"
#define GermaniumHvString           "GERMANIUM_HV"
#define GermaniumHvRbvString        "GERMANIUM_HV_RBV"
#define GermaniumHvCurrString       "GERMANIUM_HV_CURR"

//===========================================================================//

class germaniumDetector : public ADDriver {
public:
    germaniumDetector(const char *portName, int numElements, const char *ipAddress,
              int maxAddr, int numParams, int maxBuffers, size_t maxMemory,
              int interfaceMask, int interruptMask,
              int asynFlags, int autoConnect, int priority, int stackSize);

    virtual ~germaniumDetector();

    // asynPortDriver overrides
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars,
                                  size_t *nActual);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                      size_t nElements, size_t *nIn);
    virtual asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                       size_t nElements);
    virtual asynStatus readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                     size_t nElements, size_t *nIn);
    virtual asynStatus writeInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                      size_t nElements);
    virtual void report(FILE *fp, int details);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);

    // Parameter creation and initialization
    void createGermaniumParameters();
    void setGermaniumInitialValues();

    // Data array management
    void allocateDataArrays();

    // ZMQ communication (germaniumDetectorZmq.cpp)
    bool initializeZmq();
    void closeZmq();
    asynStatus zmqRegisterWrite(uint32_t addr, uint32_t value);
    asynStatus zmqRegisterRead(uint32_t addr, uint32_t *value);
    asynStatus zmqSendRecv(ZmqCommandMsg &msg, ZmqCommandMsg *reply);

    // ZMQ MARS delta-config protocol (germaniumDetectorZmq.cpp)
    // Sends lightweight field updates; Zynq assembles + loads locally.
    asynStatus zmqMarsSetGlobal(uint32_t chipMask, MarsGlobalField field, uint32_t value);
    asynStatus zmqMarsSetChannel(uint32_t channel, MarsChannelField field, uint32_t value);
    asynStatus zmqMarsLoad(uint32_t chipMask);

    void zmqDataThread();

    // PL UDP data reception (germaniumDetectorDataAcq.cpp)
    bool initializePlUdpSocket();
    void closePlUdpSocket();
    void plUdpDataThread();
    void dataProcessingThread();
    void dataWriteThread();

    // Static thread entry points
    static void zmqDataThreadC(void *pPvt);
    static void plUdpDataThreadC(void *pPvt);
    static void dataProcessingThreadC(void *pPvt);
    static void dataWriteThreadC(void *pPvt);

    // Acquisition control
    void startDataAcquisition();
    void stopDataAcquisition();

    // Event processing
    void processPhotonEvent(int element, int energy, int tdValue);
    void clearSpectra();

protected:
    // Parameter indices
    int GermaniumVER, GermaniumDETTYPE;
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
    // Data acquisition helpers
    void createDataDirectory();
    std::string generateFilename(int segmentNumber);
    bool openNewDataFile();
    void closeCurrentDataFile();
    bool writeDataToFile(const uint8_t* data, size_t dataSize);
    void addDataToWriteBuffer(const uint8_t* data, size_t dataSize);
    void flushWriteBuffer();

    // ZMQ context and sockets
    void *zmqContext;           // zmq_ctx_new()
    void *zmqControlSocket;     // REQ socket to port 5555
    void *zmqDataSocket;        // SUB socket to port 5556
    epicsMutexId zmqMutex;      // Protects zmqControlSocket (REQ is not thread-safe)
    bool zmqInitialized;

    // PL UDP data socket (raw events from FPGA)
    int plUdpSocket;
    bool plUdpInitialized;

    // Detector configuration
    int numElements;
    char ipAddress[64];
    int nchips;

    // Thread management
    epicsThreadId zmqDataThreadId;
    epicsThreadId plUdpDataThreadId;
    epicsThreadId dataProcessingThreadId;
    epicsThreadId dataWriteThreadId;
    bool threadsRunning;
    epicsEventId dataAvailable;

    // Acquisition state
    std::atomic<int> evttot;
    bool acquisitionRunning;

    // File handling
    bool fileWritingEnabled;
    int currentFileHandle;
    size_t currentFileSize;
    int currentSegmentNumber;
    std::string currentFilename;
    size_t totalBytesWritten;
    int totalFilesWritten;

    // Lock-free MPSC data queue (producers: zmqData + plUdp; consumer: dataWrite)
    DataBlock *dataQueue;                   // heap array [DATA_QUEUE_CAPACITY]
    std::atomic<uint64_t> dataQueueHead;    // next slot for producers (CAS)
    std::atomic<uint64_t> dataQueueTail;    // next slot for consumer
    epicsEventId dataWriteAvailable;

    // Spectra data — atomic for lock-free access from multiple producer
    // threads (zmqData, plUdp) and the EPICS readback thread.
    // Flat-allocated: mcaData[element * SPECTRUM_SIZE + bin]
    std::atomic<uint32_t> *mcaData;     // [numElements * SPECTRUM_SIZE]
    std::atomic<uint32_t> *tdcData;     // [numElements * TDC_SIZE]
    std::atomic<uint32_t> *countRates;  // [numElements]
    std::atomic<uint64_t> *totalCounts; // [numElements]
};

//===========================================================================//
