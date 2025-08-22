/**
 * @file GermaniumDetector.hpp
 *
 * @brief This is a driver for a Germanium detector.
 *
 * @author Ji Li
 * @orgniazation Brookhaven National Laboratory
 *
 * @created Aug 21, 2025
 *
 */
 
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cbf_ad.h>
#include <tiffio.h>

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <iocsh.h>
#include <epicsExport.h>

#include <asynOctetSyncIO.h>

#include "ADDriver.h"

#define DRIVER_VERSION      1
#define DRIVER_REVISION     0
#define DRIVER_MODIFICATION 0

/** Messages to/from detector */
#define MAX_MESSAGE_SIZE 256 
#define MAX_FILENAME_LEN 256
#define MAX_HEADER_STRING_LEN 68
#define MAX_BAD_PIXELS 100

/** Time to poll when reading from detector */
#define ASYN_POLL_TIME 1 
//#define CAMSERVER_DEFAULT_TIMEOUT 1.0

/** Additional time to wait for a camserver response after the acquire should be complete */ 
//#define CAMSERVER_ACQUIRE_TIMEOUT 10.
//#define CAMSERVER_RESET_POWER_TIMEOUT 30.

/** Time between checking to see if image file is complete */
#define FILE_READ_DELAY .01

/** Trigger modes */
typedef enum {
    TMInternal,
    TMExternalEnable,
    TMExternalTrigger,
    TMMultipleExternalTrigger,
    TMAlignment
} GermaniumTriggerMode;


static const char *gainStrings[] = {"", "", "", ""};

static const char *driverName = "GermaniumDetector";

#define GermaniumDelayTimeString          "DELAY_TIME"
#define GermaniumThresholdString          "THRESHOLD"
#define GermaniumThresholdApplyString     "THRESHOLD_APPLY"
#define GermaniumThresholdAutoApplyString "THRESHOLD_AUTO_APPLY"
#define GermaniumEnergyString             "ENERGY"
#define GermaniumArmedString              "ARMED"
#define GermaniumResetPowerString         "RESET_POWER"
#define GermaniumResetPowerTimeString     "RESET_POWER_TIME"
#define GermaniumImageFileTmotString      "IMAGE_FILE_TMOT"
#define GermaniumDetDistString            "DET_DIST"
#define GermaniumDetVOffsetString         "DET_VOFFSET"
#define GermaniumBeamXString              "BEAM_X"
#define GermaniumBeamYString              "BEAM_Y"
#define GermaniumFluxString               "FLUX"
#define GermaniumFilterTransmString       "FILTER_TRANSM"
#define GermaniumStartAngleString         "START_ANGLE"
#define GermaniumAngleIncrString          "ANGLE_INCR"
#define GermaniumDet2thetaString          "DET_2THETA"
#define GermaniumPolarizationString       "POLARIZATION"
#define GermaniumAlphaString              "ALPHA"
#define GermaniumKappaString              "KAPPA"
#define GermaniumPhiString                "PHI"
#define GermaniumPhiIncrString            "PHI_INCR"
#define GermaniumChiString                "CHI"
#define GermaniumChiIncrString            "CHI_INCR"
#define GermaniumOmegaString              "OMEGA"
#define GermaniumOmegaIncrString          "OMEGA_INCR"
#define GermaniumOscillAxisString         "OSCILL_AXIS"
#define GermaniumNumOscillString          "NUM_OSCILL"
#define GermaniumPixelCutOffString        "PIXEL_CUTOFF"
#define GermaniumThTemp0String            "TH_TEMP_0"
#define GermaniumThTemp1String            "TH_TEMP_1"
#define GermaniumThTemp2String            "TH_TEMP_2"
#define GermaniumThHumid0String           "TH_HUMID_0"
#define GermaniumThHumid1String           "TH_HUMID_1"
#define GermaniumThHumid2String           "TH_HUMID_2"
#define GermaniumTvxVersionString         "TVXVERSION"
#define GermaniumCbfTemplateFileString    "CBFTEMPLATEFILE"
#define GermaniumHeaderStringString       "HEADERSTRING"


/** Driver for Dectris Germanium pixel array detectors using their camserver server over TCP/IP socket */
class GermaniumDetector : public ADDriver {
public:
    GermaniumDetector(const char *portName, const char *camserverPort,
                    int maxSizeX, int maxSizeY,
                    int maxBuffers, size_t maxMemory,
                    int priority, int stackSize);
                 
    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, 
                                    size_t nChars, size_t *nActual);
    void report(FILE *fp, int details);
    /* These should be private but are called from C so must be public */
    void germaniumTask(); 
    
protected:
    int GermaniumDelayTime;
    #define FIRST_PILATUS_PARAM GermaniumDelayTime
    int GermaniumThreshold;
    int GermaniumThresholdApply;
    int GermaniumThresholdAutoApply;
    int GermaniumEnergy;
    int GermaniumArmed;
    int GermaniumResetPower;
    int GermaniumResetPowerTime;
    int GermaniumImageFileTmot;
    int GermaniumBadPixelFile;
    int GermaniumNumBadPixels;
    int GermaniumFlatFieldFile;
    int GermaniumMinFlatField;
    int GermaniumFlatFieldValid;
    int GermaniumGapFill;
    int GermaniumWavelength;
    int GermaniumEnergyLow;
    int GermaniumEnergyHigh;
    int GermaniumDetDist;
    int GermaniumDetVOffset;
    int GermaniumBeamX;
    int GermaniumBeamY;
    int GermaniumFlux;
    int GermaniumFilterTransm;
    int GermaniumStartAngle;
    int GermaniumAngleIncr;
    int GermaniumDet2theta;
    int GermaniumPolarization;
    int GermaniumAlpha;
    int GermaniumKappa;
    int GermaniumPhi;
    int GermaniumPhiIncr;
    int GermaniumChi;
    int GermaniumChiIncr;
    int GermaniumOmega;
    int GermaniumOmegaIncr;
    int GermaniumOscillAxis;
    int GermaniumNumOscill;
    int GermaniumPixelCutOff;  
    int GermaniumThTemp0;
    int GermaniumThTemp1;
    int GermaniumThTemp2;
    int GermaniumThHumid0;
    int GermaniumThHumid1;
    int GermaniumThHumid2;
    int GermaniumTvxVersion;
    int GermaniumCbfTemplateFile;
    int GermaniumHeaderString;

private:

    volatile bool running_ = false;

    int ctrlSock_, dataSock_;
    struct sockaddr_in ctrlDest, ctrlLocal, dataLocal;
    int ctrlPort_, dataPort_;

    epicsThreadId ctrlThread_ = nullptr;
    epicsThreadId dataThread_ = nullptr;

    /* These are the methods that are new to this class */
    void abortAcquisition();
    void makeMultipleFileFormat(const char *baseFileName);
    asynStatus waitForFileToExist(const char *fileName, epicsTimeStamp *pStartTime, double timeout, NDArray *pImage);
    void correctBadPixels(NDArray *pImage);
    int stringEndsWith(const char *aString, const char *aSubstring, int shouldIgnoreCase);
    asynStatus readImageFile(const char *fileName, epicsTimeStamp *pStartTime, double timeout, NDArray *pImage);
    asynStatus readCbf(const char *fileName, epicsTimeStamp *pStartTime, double timeout, NDArray *pImage);
    asynStatus readTiff(const char *fileName, epicsTimeStamp *pStartTime, double timeout, NDArray *pImage);
    asynStatus writeCamserver(double timeout);
    asynStatus readCamserver(double timeout);
    asynStatus writeReadCamserver(double timeout);
    asynStatus setAcquireParams();
    asynStatus setThreshold();
    asynStatus resetModulePower();
    asynStatus germaniumStatus();
    void readBadPixelFile(const char *badPixelFile);
    void readFlatFieldFile(const char *flatFieldFile);

    asynStatus writeUIntArray( asynUser* pasynUser, const uint32_t* array, size_t nelm );
   
    /* Our data */
    int imagesRemaining;
    epicsEventId startEventId;
    epicsEventId stopEventId;
    char toCamserver[MAX_MESSAGE_SIZE];
    char fromCamserver[MAX_MESSAGE_SIZE];
    NDArray *pFlatField;
    char multipleFileFormat[MAX_FILENAME_LEN];
    int multipleFileNumber;
    asynUser *pasynUserCamserver;
    badPixel badPixelMap[MAX_BAD_PIXELS];
    double averageFlatField;
    double demandedThreshold;
    double demandedEnergy;
    int firstStatusCall;
    int camserverMajor;
    int camserverMinor;
    int camserverPatch;

    GermaniumMarsConfLoad;
    GermaniumLeds;
    GermaniumMarsConfig;
    GermaniumVersion;
    GermaniumMarsCalPulse;
    GermaniumMarsPipeDelay;
    GermaniumMarsRdoutEnb;
    GermaniumEventTimeCntr;
    GermaniumSimEvtSel;
    GermaniumSimEvtRate;
    GermaniumAdcSpi;
    GermaniumCalPulseCnt;
    GermaniumCalPulseRate;
    GermaniumCalPulseWidth;
    GermaniumCalPulseMode;
    GermaniumTdCal;
    GermaniumEvtFifoData;
    GermaniumEvtFifoCnt;
    GermaniumEvtFifoCtrl;
    GermaniumIpAddr;
    GermaniumTrig;
    GermaniumCountTimeLo;
    GermaniumCountTimeHi
    GermaniumHv;
    GermaniumHvCurr;
    GermaniumTemp1;
    GermaniumTemp2;
    GermaniumTemp3;
    GermaniumZTemp;
    GermaniumDacIntRef;
    GermaniumStuffMars;
    GermaniumAdcClkSkew;
    GermaniumZddmArm;
    GermaniumDetType;

    createParam( GermaniumMarsConfLoadString,   asynParam          GermaniumMarsConfLoad );
    createParam( GermaniumLedsString,           asynParamInt32,    GermaniumLeds );
    createParam( GermaniumMarsConfigString,     asynParamInt32,    GermaniumMarsConfig );
    createParam( GermaniumVersionString,        asynParamInt32,    GermaniumVersion );
    createParam( GermaniumMarsCalPulseString,   asynParamInt32,    GermaniumMarsCalPulse );
    createParam( GermaniumMarsPipeDelayString,  asynParamInt32,    GermaniumMarsPipeDelay );
    createParam( GermaniumMarsRdoutEnbString,   asynParamInt32,    GermaniumMarsRdoutEnb );
    createParam( GermaniumEventTimeCntrString,  asynParamInt32,    GermaniumEventTimeCntr );
    createParam( GermaniumSimEvtSelString,      asynParamInt32,    GermaniumSimEvtSel );
    createParam( GermaniumSimEvtRateString,     asynParamInt32,    GermaniumSimEvtRate );
    createParam( GermaniumAdcSpiString,         asynParamInt32,    GermaniumAdcSpi );
    createParam( GermaniumCalPulseCntString,    asynParamInt32,    GermaniumCalPulseCnt );
    createParam( GermaniumCalPulseRateString,   asynParamInt32,    GermaniumCalPulseRate );
    createParam( GermaniumCalPulseWidthString,  asynParamInt32,    GermaniumCalPulseWidth );
    createParam( GermaniumCalPulseModeString,   asynParamInt32,    GermaniumCalPulseMode );
    createParam( GermaniumTdCalString,          asynParamInt32,    GermaniumTdCal );
    createParam( GermaniumEvtFifoDataString,    asynParamInt32,    GermaniumEvtFifoData );
    createParam( GermaniumEvtFifoCntString,     asynParamInt32,    GermaniumEvtFifoCnt );
    createParam( GermaniumEvtFifoCtrlString,    asynParamInt32,    GermaniumEvtFifoCtrl );
    createParam( GermaniumIpAddrString,         asynParamInt32,    GermaniumIpAddr );
    createParam( GermaniumTrigString,           asynParamInt32,    GermaniumTrig );
    createParam( GermaniumCountTimeLoString,    asynParamInt32,    GermaniumCountTimeLo );
    createParam( GermaniumCountTimeHi           asynParamInt32,    GermaniumCountTimeHi
    createParam( GermaniumHvString,             asynParamInt32,    GermaniumHv );
    createParam( GermaniumHvCurrString,         asynParamInt32,    GermaniumHvCurr );
    createParam( GermaniumTemp1String,          asynParamInt32,    GermaniumTemp1 );
    createParam( GermaniumTemp2String,          asynParamInt32,    GermaniumTemp2 );
    createParam( GermaniumTemp3String,          asynParamInt32,    GermaniumTemp3 );
    createParam( GermaniumZTempString,          asynParamInt32,    GermaniumZTemp );
    createParam( GermaniumDacIntRefString,      asynParamInt32,    GermaniumDacIntRef );
    createParam( GermaniumStuffMarsString,      asynParamInt32,    GermaniumStuffMars );
    createParam( GermaniumAdcClkSkewString,     asynParamInt32,    GermaniumAdcClkSkew );
    createParam( GermaniumZddmArmString,        asynParamInt32,    GermaniumZddmArm );
    createParam( GermaniumDetTypeString,        asynParamInt32,    GermaniumDetType );

    static constexpr uint16_t MARS_CONF_LOAD    = 0;
    static constexpr uint16_t LEDS              = 1;
    static constexpr uint16_t MARS_CONFIG       = 2;
    static constexpr uint16_t VERSIONREG        = 3;
    static constexpr uint16_t MARS_CALPULSE     = 4;
    static constexpr uint16_t MARS_PIPE_DELAY   = 5;
    static constexpr uint16_t MARS_RDOUT_ENB    = 8;
    static constexpr uint16_t EVENT_TIME_CNTR   = 9;
    static constexpr uint16_t SIM_EVT_SEL       = 10;
    static constexpr uint16_t SIM_EVENT_RATE    = 11;
    static constexpr uint16_t ADC_SPI           = 12;
    static constexpr uint16_t CALPULSE_CNT      = 16;
    static constexpr uint16_t CALPULSE_RATE     = 17;
    static constexpr uint16_t CALPULSE_WIDTH    = 18;
    static constexpr uint16_t CALPULSE_MODE     = 19;
    static constexpr uint16_t TD_CAL            = 20;
    static constexpr uint16_t EVENT_FIFO_DATA   = 24;
    static constexpr uint16_t EVENT_FIFO_CNT    = 25;
    static constexpr uint16_t EVENT_FIFO_CTRL   = 26;
    static constexpr uint16_t UDP_IP_ADDR       = 40;
    static constexpr uint16_t TRIG              = 52;
    static constexpr uint16_t COUNT_TIME        = 53;
    static constexpr uint16_t FRAME_NO          = 54;
    static constexpr uint16_t COUNT_MODE        = 55;
    static constexpr uint16_t HV                = 150;  // High voltage
    static constexpr uint16_t HV_CUR            = 151;  // High voltage current
    static constexpr uint16_t TEMP1             = 160;  // Temperature 1
    static constexpr uint16_t TEMP2             = 161;  // Temperature 2
    static constexpr uint16_t TEMP3             = 162;  // Temperature 3
    static constexpr uint16_t ZTEMP             = 170;  // CPU temperature
    static constexpr uint16_t DAC_INT_REF       = 180;  // Dac7678 internal reference

    static constexpr uint16_t STUFF_MARS        = 190;
    static constexpr uint16_t ADC_CLK_SKEW      = 191;
    static constexpr uint16_t ZDDM_ARM          = 192;

    static constexpr uint16_t DETECTOR_TYPE     = 193;
    static constexpr uint16_t COUNT_TIME_LO     = 194;
    static constexpr uint16_t COUNT_TIME_HI     = 195;
};

