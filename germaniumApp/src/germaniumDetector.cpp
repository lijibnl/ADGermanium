/**
 * @file GermaniumDetector.cpp
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

#include "GermaniumDetector.h"

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
};

void GermaniumDetector::readBadPixelFile(const char *badPixelFile)
{
    int i; 
    int xbad, ybad, xgood, ygood;
    int n;
    FILE *file;
    int nx, ny;
    const char *functionName = "readBadPixelFile";
    int numBadPixels=0;

    getIntegerParam(NDArraySizeX, &nx);
    getIntegerParam(NDArraySizeY, &ny);
    setIntegerParam(GermaniumNumBadPixels, numBadPixels);
    if (strlen(badPixelFile) == 0) return;
    file = fopen(badPixelFile, "r");
    if (file == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s, cannot open file %s\n",
            driverName, functionName, badPixelFile);
        return;
    }
    for (i=0; i<MAX_BAD_PIXELS; i++) {
        n = fscanf(file, " %d,%d %d,%d",
                  &xbad, &ybad, &xgood, &ygood);
        if (n == EOF) break;
        if (n != 4) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s, too few items =%d, should be 4\n",
                driverName, functionName, n);
            return;
        }
        this->badPixelMap[i].badIndex = ybad*nx + xbad;
        this->badPixelMap[i].replaceIndex = ygood*ny + xgood;
        numBadPixels++;
    }
    setIntegerParam(GermaniumNumBadPixels, numBadPixels);
}


void GermaniumDetector::readFlatFieldFile(const char *flatFieldFile)
{
    size_t i;
    int status;
    int ngood;
    int minFlatField;
    epicsInt32 *pData;
    const char *functionName = "readFlatFieldFile";
    NDArrayInfo arrayInfo;
    
    setIntegerParam(GermaniumFlatFieldValid, 0);
    this->pFlatField->getInfo(&arrayInfo);
    getIntegerParam(GermaniumMinFlatField, &minFlatField);
    if (strlen(flatFieldFile) == 0) return;
    status = readImageFile(flatFieldFile, NULL, 0., this->pFlatField);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s, error reading flat field file %s\n",
            driverName, functionName, flatFieldFile);
        return;
    }
    /* Compute the average counts in the flat field */
    this->averageFlatField = 0.;
    ngood = 0;
    
    for (i=0, pData = (epicsInt32 *)this->pFlatField->pData; 
         i<arrayInfo.nElements; 
         i++, pData++) {
        if (*pData < minFlatField) continue;
        ngood++;
        averageFlatField += *pData;
    }
    averageFlatField = averageFlatField/ngood;
    
    for (i=0, pData = (epicsInt32 *)this->pFlatField->pData; 
         i<arrayInfo.nElements; 
         i++, pData++) {
        if (*pData < minFlatField) *pData = (epicsInt32)averageFlatField;
    }
    /* Call the NDArray callback */
    doCallbacksGenericPointer(this->pFlatField, NDArrayData, 0);
    setIntegerParam(GermaniumFlatFieldValid, 1);
}


void GermaniumDetector::makeMultipleFileFormat(const char *baseFileName)
{
    /* This function uses the code from camserver */
    char *p, *q;
    int fmt;
    char mfTempFormat[MAX_FILENAME_LEN];
    char mfExtension[10];
    int numImages;
    
    /* baseFilename has been built by the caller.
     * Copy to temp */
    strncpy(mfTempFormat, baseFileName, sizeof(mfTempFormat));
    getIntegerParam(ADNumImages, &numImages);
    p = mfTempFormat + strlen(mfTempFormat) - 5; /* look for extension */
    if ( (q=strrchr(p, '.')) ) {
        strcpy(mfExtension, q);
        *q = '\0';
    } else {
        strcpy(mfExtension, ""); /* default is raw image */
    }
    multipleFileNumber=0;   /* start number */
    fmt=5;        /* format length */
    if ( !(p=strrchr(mfTempFormat, '/')) ) {
        p=mfTempFormat;
    }
    if ( (q=strrchr(p, '_')) ) {
        q++;
        if (isdigit(*q) && isdigit(*(q+1)) && isdigit(*(q+2))) {
            multipleFileNumber=atoi(q);
            fmt=0;
            p=q;
            while(isdigit(*q)) {
                fmt++;
                q++;
            }
            *p='\0';
            if (((fmt<3)  || ((fmt==3) && (numImages>999))) || 
                ((fmt==4) && (numImages>9999))) { 
                fmt=5;
            }
        } else if (*q) {
            strcat(p, "_"); /* force '_' ending */
        }
    } else {
        strcat(p, "_"); /* force '_' ending */
    }
    /* Build the final format string */
    epicsSnprintf(this->multipleFileFormat, sizeof(this->multipleFileFormat), "%s%%.%dd%s",
                  mfTempFormat, fmt, mfExtension);
}

/** This function waits for the specified file to exist.  It checks to make sure that
 * the creation time of the file is after a start time passed to it, to force it to wait
 * for a new file to be created.
 */
asynStatus GermaniumDetector::waitForFileToExist(const char *fileName, epicsTimeStamp *pStartTime, double timeout, NDArray *pImage)
{
    int fd=-1;
    int fileExists=0;
    struct stat statBuff;
    epicsTimeStamp tStart, tCheck;
    time_t acqStartTime;
    double deltaTime=0.;
    int status=-1;
    const char *functionName = "waitForFileToExist";

    if (pStartTime) epicsTimeToTime_t(&acqStartTime, pStartTime);
    epicsTimeGetCurrent(&tStart);

    while (deltaTime <= timeout) {
        fd = open(fileName, O_RDONLY, 0);
        if ((fd >= 0) && (timeout != 0.)) {
            fileExists = 1;
            /* The file exists.  Make sure it is a new file, not an old one.
             * We don't do this check if timeout==0, which is used for reading flat field files */
            status = fstat(fd, &statBuff);
            if (status){
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling fstat, errno=%d %s\n",
                    driverName, functionName, errno, fileName);
                close(fd);
                return(asynError);
            }
            /* We allow up to 10 second clock skew between time on machine running this IOC
             * and the machine with the file system returning modification time */
            if (difftime(statBuff.st_mtime, acqStartTime) > -10) break;
            close(fd);
            fd = -1;
        }
        /* Sleep, but check for stop event, which can be used to abort a long acquisition */
        unlock();
        status = epicsEventWaitWithTimeout(this->stopEventId, FILE_READ_DELAY);
        lock();
        if (status == epicsEventWaitOK) {
            setStringParam(ADStatusMessage, "Acquisition aborted");
            setIntegerParam(ADStatus, ADStatusAborted);
            return(asynError);
        }
        epicsTimeGetCurrent(&tCheck);
        deltaTime = epicsTimeDiffInSeconds(&tCheck, &tStart);
    }
    if (fd < 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s timeout waiting for file to be created %s\n",
            driverName, functionName, fileName);
        if (fileExists) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "  file exists but is more than 10 seconds old, possible clock synchronization problem\n");
            setStringParam(ADStatusMessage, "Image file is more than 10 seconds old");
        } else
            setStringParam(ADStatusMessage, "Timeout waiting for file to be created");
        return(asynError);
    }
    close(fd);
    return(asynSuccess);
}



/** This function reads TIFF or CBF image files.  It is not intended to be general, it
 * is intended to read the TIFF or CBF files that camserver creates.  It checks to make
 * sure that the creation time of the file is after a start time passed to it, to force
 * it to wait for a new file to be created.
 */
asynStatus GermaniumDetector::readImageFile(const char *fileName, epicsTimeStamp *pStartTime, double timeout, NDArray *pImage)
{
    const char *functionName = "readImageFile";

    if (stringEndsWith(fileName, ".tif", 1) || stringEndsWith(fileName, ".tiff", 1)) {
        return readTiff(fileName, pStartTime, timeout, pImage);
    } else if (stringEndsWith(fileName, ".cbf", 1)) {
        return readCbf(fileName, pStartTime, timeout, pImage);
    } else {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s, unsupported image file name extension, expected .tif or .cbf, fileName=%s\n",
            driverName, functionName, fileName);
        setStringParam(ADStatusMessage, "Unsupported file extension, expected .tif or .cbf");
        return(asynError);
    }
}


asynStatus GermaniumDetector::setAcquireParams()
{
    int ival;
    double dval;
    int triggerMode;
    asynStatus status;
    char *substr = NULL;
    int pixelCutOff = 0;
    
    status = getIntegerParam(ADTriggerMode, &triggerMode);
    if (status != asynSuccess) triggerMode = TMInternal;
    
     /* When we change modes download all exposure parameters, since some modes
      * replace values with new parameters */
    if (triggerMode == TMAlignment) {
        setIntegerParam(ADNumImages, 1);
    }
    
    status = getIntegerParam(ADNumImages, &ival);
    if ((status != asynSuccess) || (ival < 1)) {
        ival = 1;
        setIntegerParam(ADNumImages, ival);
    }
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "nimages %d", ival);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT); 

    status = getIntegerParam(ADNumExposures, &ival);
    if ((status != asynSuccess) || (ival < 1)) {
        ival = 1;
        setIntegerParam(ADNumExposures, ival);
    }
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "nexpframe %d", ival);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT); 

    status = getDoubleParam(ADAcquireTime, &dval);
    if ((status != asynSuccess) || (dval < 0.)) {
        dval = 1.;
        setDoubleParam(ADAcquireTime, dval);
    }
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "exptime %11.8f", dval);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);

    status = getDoubleParam(ADAcquirePeriod, &dval);
    if ((status != asynSuccess) || (dval < 0.)) {
        dval = 2.;
        setDoubleParam(ADAcquirePeriod, dval);
    }
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "expperiod %11.8f", dval);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);

    status = getDoubleParam(GermaniumDelayTime, &dval);
    if ((status != asynSuccess) || (dval < 0.)) {
        dval = 0.;
        setDoubleParam(GermaniumDelayTime, dval);
    }
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "delay %f", dval);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);

    status = getIntegerParam(GermaniumGapFill, &ival);
    if ((status != asynSuccess) || (ival < -2) || (ival > 0)) {
        ival = -2;
        setIntegerParam(GermaniumGapFill, ival);
    }
    /* -2 is used to indicate that GapFill is not supported because it is a single element detector */
    if (ival != -2) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "gapfill %d", ival);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT); 
    }

    /* Read back the pixel count rate cut off value. */
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "Tau");
    status=writeReadCamserver(5.0); 

    /* Response contains the string "cutoff = 1221026 counts"*/
    if (!status) {
        if ((substr = strstr(this->fromCamserver, "cutoff")) != NULL) {
            sscanf(substr, "cutoff = %d counts", &pixelCutOff);
            setIntegerParam(GermaniumPixelCutOff, pixelCutOff);
        }
    }
   
    return(asynSuccess);

}

asynStatus GermaniumDetector::setThreshold()
{
    int igain, status;
    double threshold, dgain, energy;
    char *substr = NULL;
    int threshold_readback = 0;
    int energy_readback = 0;
    
    getDoubleParam(ADGain, &dgain);
    igain = (int)(dgain + 0.5);
    if (igain < 0) igain = 0;
    if (igain > 3) igain = 3;
    threshold = this->demandedThreshold;
    energy = this->demandedEnergy;
    if (energy == 0.) energy = threshold * 2.;
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "SetThreshold energy %.0f %s %.0f", 
                  energy*1000., gainStrings[igain], threshold*1000.);
    /* Set the status to waiting so we can be notified when it has finished */
    setIntegerParam(ADStatus, ADStatusWaiting);
    setStringParam(ADStatusMessage, "Setting threshold");
    callParamCallbacks();
    
    status=writeReadCamserver(110.0);  /* This command can take 96 seconds on a 6M */
    if (status)
        setIntegerParam(ADStatus, ADStatusError);
    else
        setIntegerParam(ADStatus, ADStatusIdle);
    setIntegerParam(GermaniumThresholdApply, 0);

    /* Read back the actual threshold setting, in case we are out of bounds. */
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "SetThreshold");
    status=writeReadCamserver(5.0); 

    /* Response should contain "threshold: 9000 eV; vcmp:"*/
    if (!status) {
        if ((substr = strstr(this->fromCamserver, "threshold: ")) != NULL) {
            sscanf(strtok(substr, ";"), "threshold: %d eV", &threshold_readback);
            setDoubleParam(GermaniumThreshold, (double)threshold_readback/1000.0);
        }
    }
    
    /* Read back the actual energy setting. */
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "SetEnergy");
    status=writeReadCamserver(5.0); 

    /* Response should contain "threshold: 9000 eV; vcmp:"*/
    if (!status) {
        sscanf(this->fromCamserver, "15 OK Energy setting: %d eV", &energy_readback);
            setDoubleParam(GermaniumEnergy, (double)energy_readback/1000.0);
    }

    /* The SetThreshold command resets numimages to 1 and gapfill to 0, so re-send current
     * acquisition parameters */
    setAcquireParams();

    callParamCallbacks();

    return(asynSuccess);
}

asynStatus GermaniumDetector::resetModulePower()
{
    int resetTime;
    static const char *functionName="resetModulePower";

    // This command only exists on camserver 7.9.0 and higher
    if ((camserverMajor < 7) || ((camserverMajor == 7) && (camserverMinor < 9))) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s ResetModulePower not supported on version %d.%d.%d of camserver\n",
            driverName, functionName, camserverMajor, camserverMinor, camserverPatch);
        return asynError;
    }
    setStringParam(ADStatusMessage, "Resetting module power");
    callParamCallbacks();
    getIntegerParam(GermaniumResetPowerTime, &resetTime);
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "ResetModulePower %d", resetTime);
    writeReadCamserver(CAMSERVER_RESET_POWER_TIMEOUT + resetTime);
    // Need to set the threshold after resetting module power
    setThreshold();
    return asynSuccess;
}

asynStatus GermaniumDetector::writeCamserver(double timeout)
{
    size_t nwrite;
    asynStatus status;
    const char *functionName="writeCamserver";

    /* Flush any stale input, since the next operation is likely to be a read */
    status = pasynOctetSyncIO->flush(this->pasynUserCamserver);
    status = pasynOctetSyncIO->write(this->pasynUserCamserver, this->toCamserver,
                                     strlen(this->toCamserver), timeout,
                                     &nwrite);
                                        
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s, status=%d, sent\n%s\n",
                    driverName, functionName, status, this->toCamserver);

    /* Set output string so it can get back to EPICS */
    setStringParam(ADStringToServer, this->toCamserver);
    
    return(status);
}


asynStatus GermaniumDetector::readCamserver(double timeout)
{
    size_t nread;
    asynStatus status=asynSuccess;
    int eventStatus;
    asynUser *pasynUser = this->pasynUserCamserver;
    int eomReason;
    epicsTimeStamp tStart, tCheck;
    double deltaTime;
    const char *functionName="readCamserver";

    /* We implement the timeout with a loop so that the port does not
     * block during the entire read.  If we don't do this then it is not possible
     * to abort a long exposure */
    deltaTime = 0;
    epicsTimeGetCurrent(&tStart);
    while (deltaTime <= timeout) {
        unlock();
        status = pasynOctetSyncIO->read(pasynUser, this->fromCamserver,
                                        sizeof(this->fromCamserver), ASYN_POLL_TIME,
                                        &nread, &eomReason);
        /* Check for an abort event sent during a read. Otherwise we can miss it and mess up the next acqusition.*/
        eventStatus = epicsEventWaitWithTimeout(this->stopEventId, 0.001);
        lock();
        if (eventStatus == epicsEventWaitOK) {
            setStringParam(ADStatusMessage, "Acquisition aborted");
            setIntegerParam(ADStatus, ADStatusAborted);
            return(asynError);
        }
        if (status != asynTimeout) break;

        /* Sleep, but check for stop event, which can be used to abort a long acquisition */
        unlock();
        eventStatus = epicsEventWaitWithTimeout(this->stopEventId, ASYN_POLL_TIME);
        lock();
        if (eventStatus == epicsEventWaitOK) {
            setStringParam(ADStatusMessage, "Acquisition aborted");
            setIntegerParam(ADStatus, ADStatusAborted);
            return(asynError);
        }
        epicsTimeGetCurrent(&tCheck);
        deltaTime = epicsTimeDiffInSeconds(&tCheck, &tStart);
    }

    // If we got asynTimeout, and timeout=0 then this is not an error, it is a poll checking for possible reply and we are done
   if ((status == asynTimeout) && (timeout == 0)) return(asynSuccess);
   if (status != asynSuccess)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s:%s, timeout=%f, status=%d received %lu bytes\n%s\n",
                    driverName, functionName, timeout, status, (unsigned long)nread, this->fromCamserver);
   else {
        /* Look for the string OK in the response */
        if (!strstr(this->fromCamserver, "OK")) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "%s:%s unexpected response from camserver, no OK, response=%s\n",
                      driverName, functionName, this->fromCamserver);
            setStringParam(ADStatusMessage, "Error from camserver");
            status = asynError;
        } else
            setStringParam(ADStatusMessage, "Camserver returned OK");
    }

    /* Set output string so it can get back to EPICS */
    setStringParam(ADStringFromServer, this->fromCamserver);

    return(status);
}

asynStatus GermaniumDetector::writeReadCamserver(double timeout)
{
    asynStatus status;
    
    status = writeCamserver(timeout);
    if (status) return status;
    status = readCamserver(timeout);
    return status;
}

static void germaniumTaskC(void *drvPvt)
{
    GermaniumDetector *pPvt = (GermaniumDetector *)drvPvt;
    
    pPvt->germaniumTask();
}

/** This thread controls acquisition, reads image files to get the image data, and
  * does the callbacks to send it to higher layers */
void GermaniumDetector::germaniumTask()
{
    int status = asynSuccess;
    int imageCounter;
    int numImages;
    int numExposures;
    int multipleFileNextImage=0;  /* This is the next image number, starting at 0 */
    int acquire;
    ADStatus_t acquiring;
    double startAngle;
    NDArray *pImage;
    double acquireTime, acquirePeriod;
    double readImageFileTimeout, timeout;
    int triggerMode;
    epicsTimeStamp startTime;
    const char *functionName = "germaniumTask";
    char headerString[MAX_HEADER_STRING_LEN];
    char fullFileName[MAX_FILENAME_LEN];
    char filePath[MAX_FILENAME_LEN];
    char statusMessage[MAX_MESSAGE_SIZE];
    size_t dims[2];
    int itemp;
    int arrayCallbacks;
    int flatFieldValid;
    int aborted = 0;
    int statusParam = 0;

    this->lock();

    /* Loop forever */
    while (1) {
        /* Is acquisition active? */
        getIntegerParam(ADAcquire, &acquire);

        /* If we are not acquiring then wait for a semaphore that is given when acquisition is started */
        if ((aborted) || (!acquire)) {
            /* Only set the status message if we didn't encounter any errors last time, so we don't overwrite the 
             error message */
            if (!status)
            setStringParam(ADStatusMessage, "Waiting for acquire command");
            callParamCallbacks();
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            this->unlock();
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: waiting for acquire to start\n", driverName, functionName);
            status = epicsEventWait(this->startEventId);
            this->lock();
            aborted = 0;
            acquire = 1;
        }
        
        /* We are acquiring. */
        /* Get the current time */
        epicsTimeGetCurrent(&startTime);
        
        /* Get the exposure parameters */
        getDoubleParam(ADAcquireTime, &acquireTime);
        getDoubleParam(ADAcquirePeriod, &acquirePeriod);
        getDoubleParam(GermaniumImageFileTmot, &readImageFileTimeout);
        
        /* Get the acquisition parameters */
        getIntegerParam(ADTriggerMode, &triggerMode);
        getIntegerParam(ADNumImages, &numImages);
        getIntegerParam(ADNumExposures, &numExposures);
        
        acquiring = ADStatusAcquire;
        setIntegerParam(ADStatus, acquiring);

        /* Reset the MX settings start angle */
        getDoubleParam(GermaniumStartAngle, &startAngle);
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Start_angle %f", startAngle);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);

            case TMExternalTrigger:
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                    "ExtTrigger %s", fullFileName);
                break;
            case TMMultipleExternalTrigger:
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                    "ExtMTrigger %s", fullFileName);
                break;
            case TMAlignment:
                getStringParam(NDFilePath, sizeof(filePath), filePath);
                epicsSnprintf(fullFileName, sizeof(fullFileName), "%salignment.tif", 
                              filePath);
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                    "Exposure %s", fullFileName);
                break;
        }
        setStringParam(ADStatusMessage, "Starting exposure");
        /* Send the acquire command to camserver and wait for the 15OK response */
        writeReadCamserver(2.0);
        /* Do another read in case there is an ERR string at the end of the input buffer */
        status=readCamserver(0.0);

        /* If the status wasn't asynSuccess or asynTimeout, report the error */
        if (status>1) {
            acquire = 0;
        }
        else {
            /* Set status back to asynSuccess as the timeout was expected */
            status = asynSuccess;
            /* Open the shutter */
            setShutter(1);
            /* Set the armed flag */
            setIntegerParam(PilatusArmed, 1);
            /* Create the format string for constructing file names for multi-image collection */
            makeMultipleFileFormat(fullFileName);
            multipleFileNextImage = 0;
            /* Call the callbacks to update any changes */
            setStringParam(NDFullFileName, fullFileName);
            callParamCallbacks();
        }

        while (acquire) {
            if (numImages == 1) {
                /* For single frame or alignment mode need to wait for 7OK response from camserver
                 * saying acquisition is complete before trying to read file, else we get a
                 * recent but stale file. */
                setStringParam(ADStatusMessage, "Waiting for 7OK response");
                callParamCallbacks();
                timeout = ((numExposures-1) * acquirePeriod) + acquireTime;
                status = readCamserver(timeout + CAMSERVER_ACQUIRE_TIMEOUT);
                /* If there was an error jump to bottom of loop */
                if (status) {
                    acquire = 0;
                    aborted = 1;
                    if(status==asynTimeout) {
                        setStringParam(ADStatusMessage, "Timeout waiting for camserver response");
                        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "camcmd k");
                        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
                        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "K");
                        writeCamserver(CAMSERVER_DEFAULT_TIMEOUT);
                    }
                    continue;
                }
            } else {
                /* If this is a multi-file acquisition the file name is built differently */
                epicsSnprintf(fullFileName, sizeof(fullFileName), multipleFileFormat, 
                              multipleFileNumber);
                setStringParam(NDFullFileName, fullFileName);
            }
            getIntegerParam(NDArrayCallbacks, &arrayCallbacks);

            if (arrayCallbacks) {
                /* Get an image buffer from the pool */
                getIntegerParam(ADMaxSizeX, &itemp); dims[0] = itemp;
                getIntegerParam(ADMaxSizeY, &itemp); dims[1] = itemp;
                pImage = this->pNDArrayPool->alloc(2, dims, NDInt32, 0, NULL);
                epicsSnprintf(statusMessage, sizeof(statusMessage), "Reading image file %s", fullFileName);
                setStringParam(ADStatusMessage, statusMessage);
                callParamCallbacks();
                /* We release the mutex when calling readImageFile, because this takes a long time and
                 * we need to allow abort operations to get through */
                status = readImageFile(fullFileName, &startTime, 
                                       (numExposures * acquireTime) + readImageFileTimeout, 
                                       pImage); 
                /* If there was an error jump to bottom of loop */
                if (status) {
                    acquire = 0;
                    aborted = 1;
                    pImage->release();
                    continue;
                }

                /* We successfully read an image - increment the array counter */
                getIntegerParam(NDArrayCounter, &imageCounter);
                imageCounter++;
                setIntegerParam(NDArrayCounter, imageCounter);
                /* Call the callbacks to update any changes */
                callParamCallbacks();

                /* Now assemble the NDArray */
                getIntegerParam(PilatusFlatFieldValid, &flatFieldValid);
                if (flatFieldValid) {
                    epicsInt32 *pData, *pFlat;
                    size_t i;
                    for (i=0, pData = (epicsInt32 *)pImage->pData, pFlat = (epicsInt32 *)this->pFlatField->pData;
                         i<dims[0]*dims[1]; 
                         i++, pData++, pFlat++) {
                        *pData = (epicsInt32)((this->averageFlatField * *pData) / *pFlat);
                    }
                } 
                /* Put the frame number and time stamp into the buffer */
                pImage->uniqueId = imageCounter;
                updateTimeStamps(pImage);

                /* Get any attributes that have been defined for this driver */        
                this->getAttributes(pImage->pAttributeList);
                
                /* Call the NDArray callback */
                asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                     "%s:%s: calling NDArray callback\n", driverName, functionName);
                doCallbacksGenericPointer(pImage, NDArrayData, 0);
                /* Free the image buffer */
                pImage->release();
            }
            if (numImages == 1) {
                if (triggerMode == TMAlignment) {
                    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                        "Exposure %s", fullFileName);
                    /* Send the acquire command to camserver and wait for the 15OK response */
                    writeReadCamserver(2.0);
                } else {
                    acquire = 0;
                }
            } else if (numImages > 1) {
                multipleFileNextImage++;
                multipleFileNumber++;
                if (multipleFileNextImage == numImages) acquire = 0;
            }
            
        }
        /* We are done acquiring */
        /* Wait for the 7OK response from camserver in the case of multiple images */
        if ((numImages > 1) && (status == asynSuccess)) {
            /* If arrayCallbacks is 0 we will have gone through the above loop without waiting
             * for each image file to be written.  Thus, we may need to wait a long time for
             * the 7OK response.  
             * If arrayCallbacks is 1 then the response should arrive fairly soon. */
            if (arrayCallbacks) 
                    acquire = 0;
                }
            } else if (numImages > 1) {
                multipleFileNextImage++;
                multipleFileNumber++;
                if (multipleFileNextImage == numImages) acquire = 0;
            }
            
        }
        /* We are done acquiring */
        /* Wait for the 7OK response from camserver in the case of multiple images */
        if ((numImages > 1) && (status == asynSuccess)) {
            /* If arrayCallbacks is 0 we will have gone through the above loop without waiting
             * for each image file to be written.  Thus, we may need to wait a long time for
             * the 7OK response.  
             * If arrayCallbacks is 1 then the response should arrive fairly soon. */
            if (arrayCallbacks) 
                timeout = readImageFileTimeout;
            else 
                timeout = (numImages * numExposures * acquirePeriod) + CAMSERVER_ACQUIRE_TIMEOUT;
            setStringParam(ADStatusMessage, "Waiting for 7OK response");
            callParamCallbacks();
            status = readCamserver(timeout);
            /* In the case of a timeout, camserver could still be acquiring. So we need to send a stop.*/
            if (status == asynTimeout) {
                setStringParam(ADStatusMessage, "Timeout waiting for camserver response");
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "camcmd k");
                writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "K");
                writeCamserver(CAMSERVER_DEFAULT_TIMEOUT);
                aborted = 1;
            }
        }

        /* If everything was ok, set the status back to idle */
        getIntegerParam(ADStatus, &statusParam);
        if (!status) {
            setIntegerParam(ADStatus, ADStatusIdle);
        } else {
            if (statusParam != ADStatusAborted) {
                setIntegerParam(ADStatus, ADStatusError);
            }
        }

        /* Call the callbacks to update any changes */
        callParamCallbacks();

        setShutter(0);
        setIntegerParam(ADAcquire, 0);
        setIntegerParam(GermaniumArmed, 0);

        /* Call the callbacks to update any changes */
        callParamCallbacks();        
    }
}

/** This function is called periodically read the detector status (temperature, humidity, etc.)
    It should not be called if we are acquiring data, to avoid polling camserver when taking data.*/
asynStatus GermaniumDetector::germaniumStatus()
{
  asynStatus status = asynSuccess;
  float temp = 0.0;
  float humid = 0.0;
  char *substr = NULL;

  /* Read the camserver version once.*/
  if (firstStatusCall) {
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "version");
    status=writeReadCamserver(1.0);
    if (!status) {
      // Old versions return strings like "Code release:  tvx-7.3.13-121212"
      // New versions return strings like "Code release: 7.9.0"
      // The start of the firmware version is 1 character past the last space
      substr = strrchr(this->fromCamserver, ' ') + 1;
      setStringParam(GermaniumTvxVersion, substr);
      setStringParam(ADSDKVersion, substr);
      if (substr[0] == 't') substr += 4;
      sscanf(substr, "%d.%d.%d", &camserverMajor, &camserverMinor, &camserverPatch);
      setIntegerParam(ADStatus, ADStatusIdle);
    } else {
      setIntegerParam(ADStatus, ADStatusError);
    }
    firstStatusCall = 0;
  }

  /* Read temp and humidity.*/
  epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "thread");
  status=writeReadCamserver(1.0); 
  /* Response should contain: 
     Channel 0: Temperature = 31.4C, Rel. Humidity = 22.1%;\n
     Channel 1: Temperature = 25.8C, Rel. Humidity = 33.5%;\n
     Channel 2: Temperature = 28.6C, Rel. Humidity = 2.0%
     However, not every detector has all 3 channels.*/

  if (!status) {

    if ((substr = strstr(this->fromCamserver, "Channel 0")) != NULL) {
      sscanf(substr, "Channel 0: Temperature = %fC, Rel. Humidity = %f", &temp, &humid);
      setDoubleParam(GermaniumThTemp0, temp);
      setDoubleParam(GermaniumThHumid0, humid);
      setDoubleParam(ADTemperature, temp);
    }
    if ((substr = strstr(this->fromCamserver, "Channel 1")) != NULL) {
        sscanf(substr, "Channel 1: Temperature = %fC, Rel. Humidity = %f", &temp, &humid);
        setDoubleParam(GermaniumThTemp1, temp);
        setDoubleParam(GermaniumThHumid1, humid);
    }
    if ((substr = strstr(this->fromCamserver, "Channel 2")) != NULL) {
        sscanf(substr, "Channel 2: Temperature = %fC, Rel. Humidity = %f", &temp, &humid);
        setDoubleParam(GermaniumThTemp2, temp);
        setDoubleParam(GermaniumThHumid2, humid);
    }
    if ((substr = strstr(this->fromCamserver, "Channel 3")) != NULL) {
        sscanf(substr, "Channel 3: Temperature = %fC, Rel. Humidity = %f", &temp, &humid);
        setDoubleParam(GermaniumThTemp0, temp);
        setDoubleParam(GermaniumThHumid0, humid);
    }

  } else {
    setIntegerParam(ADStatus, ADStatusError);
  }      
  callParamCallbacks();
  return status;
}



/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, ADTriggerMode, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus GermaniumDetector::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int adstatus;
    asynStatus status = asynSuccess;
    const char *functionName = "writeInt32";

    /* Ensure that ADStatus is set correctly before we set ADAcquire.*/
    getIntegerParam(ADStatus, &adstatus);
    if (function == ADAcquire) {
      if (value && ((adstatus == ADStatusIdle) || adstatus == ADStatusError || adstatus == ADStatusAborted)) {
        setStringParam(ADStatusMessage, "Acquiring data");
        setIntegerParam(ADStatus, ADStatusAcquire);
      }
      if (!value && (adstatus == ADStatusAcquire)) {
        setStringParam(ADStatusMessage, "Acquisition aborted");
        setIntegerParam(ADStatus, ADStatusAborted);
      }
    }
    callParamCallbacks();

    status = setIntegerParam(function, value);

    if (function == ADAcquire) {
        if (value && (adstatus == ADStatusIdle || adstatus == ADStatusError || adstatus == ADStatusAborted)) {
            /* Send an event to wake up the Germanium task.  */
            epicsEventSignal(this->startEventId);
        } 
        if (!value && (adstatus == ADStatusAcquire)) {
          /* This was a command to stop acquisition */
            epicsEventSignal(this->stopEventId);
            epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "camcmd k");
            writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
            epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "K");
            writeCamserver(CAMSERVER_DEFAULT_TIMEOUT);
            /* Sleep for two seconds to allow acqusition to stop in camserver.*/
            epicsThreadSleep(2);
            setStringParam(ADStatusMessage, "Acquisition aborted");
        }
    } else if ((function == ADTriggerMode) ||
               (function == ADNumImages) ||
               (function == ADNumExposures) ||
               (function == GermaniumGapFill)) {
        setAcquireParams();
    } else if (function == GermaniumThresholdApply) {
        setThreshold();
    } else if (function == GermaniumResetPower) {
        resetModulePower();
     } else if (function == GermaniumNumOscill) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings N_oscillations %d", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == ADReadStatus) {
        if (adstatus != ADStatusAcquire) {
          status = germaniumStatus();
        }
    } else { 
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_PILATUS_PARAM) status = ADDriver::writeInt32(pasynUser, value);
    }
            
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    
    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d function=%d, value=%d\n", 
              driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%d\n", 
              driverName, functionName, function, value);
    return status;
}


/** Called when asyn clients call pasynFloat64->write().
  * This function performs actions for some parameters, including ADAcquireTime, ADGain, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus GermaniumDetector::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    double energyLow, energyHigh;
    double beamX, beamY;
    int thresholdAutoApply;
    const char *functionName = "writeFloat64";
    double oldValue;

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    getDoubleParam(function, &oldValue);
    status = setDoubleParam(function, value);
    

    /* Changing any of the following parameters requires recomputing the base image */
    if ((function == ADGain) ||
        (function == GermaniumEnergy) ||
        (function == GermaniumThreshold)) {
        getIntegerParam(GermaniumThresholdAutoApply, &thresholdAutoApply);
        if (function == GermaniumThreshold) {
            this->demandedThreshold = value;
        }
        if (function == GermaniumEnergy) {
            this->demandedEnergy = value;
        }
        if (thresholdAutoApply) {
          if (function == GermaniumThreshold) {
            status = setDoubleParam(function, this->demandedThreshold);
          }
          if (function == GermaniumEnergy) {
            status = setDoubleParam(function, this->demandedEnergy);
          }
          setThreshold();
        } else {
          /* Set the old value back if we are deferring setting the threshold.*/
          if (function == GermaniumThreshold) {
            status = setDoubleParam(function, oldValue);
          }
          if (function == GermaniumEnergy) {
            status = setDoubleParam(function, oldValue);
          }
        }
    } else if ((function == ADAcquireTime) ||
               (function == ADAcquirePeriod) ||
               (function == GermaniumDelayTime)) {
        setAcquireParams();
    } else if (function == GermaniumWavelength) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Wavelength %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if ((function == GermaniumEnergyLow) ||
               (function == GermaniumEnergyHigh)) {
        getDoubleParam(GermaniumEnergyLow, &energyLow);
        getDoubleParam(GermaniumEnergyHigh, &energyHigh);
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Energy_range %f,%f", energyLow, energyHigh);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumDetDist) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Detector_distance %f", value / 1000.0);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumDetVOffset) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Detector_Voffset %f", value / 1000.0);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if ((function == GermaniumBeamX) ||
               (function == GermaniumBeamY)) {
        getDoubleParam(GermaniumBeamX, &beamX);
        getDoubleParam(GermaniumBeamY, &beamY);
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Beam_xy %f,%f", beamX, beamY);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumFlux) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Flux %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumFilterTransm) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Filter_transmission %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumStartAngle) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Start_angle %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumAngleIncr) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Angle_increment %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumDet2theta) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Detector_2theta %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumPolarization) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Polarization %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumAlpha) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Alpha %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumKappa) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Kappa %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumPhi) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Phi %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumPhiIncr) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Phi_increment %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumChi) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Chi %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumChiIncr) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Chi_increment %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumOmega) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Omega %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumOmegaIncr) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Omega_increment %f", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_PILATUS_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
    }

    if (status) {
        /* Something went wrong so we set the old value back */
        setDoubleParam(function, oldValue);
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s error, status=%d function=%d, value=%f\n", 
              driverName, functionName, status, function, value);
    }
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%f\n", 
              driverName, functionName, function, value);
    
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    return status;
}

/** Called when asyn clients call pasynOctet->write().
  * This function performs actions for some parameters, including GermaniumBadPixelFile, ADFilePath, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the string to write.
  * \param[in] nChars Number of characters to write.
  * \param[out] nActual Number of characters actually written. */
asynStatus GermaniumDetector::writeOctet(asynUser *pasynUser, const char *value, 
                                    size_t nChars, size_t *nActual)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeOctet";

    /* Set the parameter in the parameter library. */
    status = (asynStatus)setStringParam(function, (char *)value);

    if (function == GermaniumBadPixelFile) {
        this->readBadPixelFile(value);
    } else if (function == GermaniumFlatFieldFile) {
        this->readFlatFieldFile(value);
    } else if (function == NDFilePath) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "imgpath %s", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
        this->checkPath();
    } else if (function == GermaniumOscillAxis) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings Oscillation_axis %s",
            strlen(value) == 0 ? "(nil)" : value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else if (function == GermaniumCbfTemplateFile) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "mxsettings cbf_template_file %s",
            strlen(value) == 0 ? "0" : value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_PILATUS_PARAM) status = ADDriver::writeOctet(pasynUser, value, nChars, nActual);
    }
    
     /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks();

    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, value=%s", 
                  driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%s\n", 
              driverName, functionName, function, value);
    *nActual = nChars;
    return status;
}

asynStatus GermaniumDetector::WriteUIntArray( asynUser* pasynUser
                                            , const uint32_t* array
                                            , size_t nelm
                                            )
{
    int function = p_asyn_user->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "WriteUIntArray";

    status = (asynStatus)setStringParam( function, (char*)value );

    if ( function == GermaniumLoadZddm )
    {
        size_t sent = sendto( ctrl_sock_
                            , msg
                            , 4+4*nelm
                            , 0
                            , (struct sockaddr*) &dest
                            , sizeof(dest)
                            );
        if ( sent < 0 )
            perror( "sendto" );
    }
    else
        perror( "WriteUIntArray" );

    return status;
}



/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void GermaniumDetector::report(FILE *fp, int details)
{

    fprintf(fp, "Germanium detector %s\n", this->portName);
    if (details > 0) {
        int nx, ny, dataType;
        getIntegerParam(ADSizeX, &nx);
        getIntegerParam(ADSizeY, &ny);
        getIntegerParam(NDDataType, &dataType);
        fprintf(fp, "  NX, NY:            %d  %d\n", nx, ny);
        fprintf(fp, "  Data type:         %d\n", dataType);
    }
    /* Invoke the base class method */
    ADDriver::report(fp, details);
}

extern "C" int GermaniumDetectorConfig(const char *portName, const char *camserverPort, 
                                    int maxSizeX, int maxSizeY,
                                    int maxBuffers, size_t maxMemory,
                                    int priority, int stackSize)
{
    new GermaniumDetector(portName, camserverPort, maxSizeX, maxSizeY, maxBuffers, maxMemory,
                        priority, stackSize);
    return(asynSuccess);
}

/** Constructor for Germanium driver; most parameters are simply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to collect the detector data, 
  * and sets reasonable default values for the parameters defined in this class, asynNDArrayDriver, and ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] camserverPort The name of the asyn port previously created with drvAsynIPPortConfigure to
  *            communicate with camserver.
  * \param[in] maxSizeX The size of the Germanium detector in the X direction.
  * \param[in] maxSizeY The size of the Germanium detector in the Y direction.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
GermaniumDetector::GermaniumDetector(const char *portName, const char *camserverPort,
                                int maxSizeX, int maxSizeY,
                                int maxBuffers, size_t maxMemory,
                                int priority, int stackSize)

    : ADDriver(portName, 1, 0, maxBuffers, maxMemory,
               0, 0,             /* No interfaces beyond those set in ADDriver.cpp */
               ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize),
      imagesRemaining(0), firstStatusCall(1)

{
    int status = asynSuccess;
    char versionString[20];
    const char *functionName = "GermaniumDetector";
    size_t dims[2];



    // Create UDP sockets
    ctrl_sock_ = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( ctrl_sock < 0 )
    {
        perror( "socket" );
    }

    int optval = 1;
    setsockopt( ctrl_sock_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) );

    memset( &dest_, 0, sizeof( dest_ );
    dest_.sin_family = AF_INET;
    dest_.sin_port = htons( ctrl_port_ );
    if (inet_aton(ip, &dest_.sin_addr) == 0) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        close(sock);
        return;
    }


    data_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if ( data_sock_ < 0 )
    {
        perror("socket");
        return -1;
    }


    /* Create the epicsEvents for signaling to the germanium task when acquisition starts and stops */
    this->startEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->startEventId) {
        printf("%s:%s epicsEventCreate failure for start event\n", 
            driverName, functionName);
        return;
    }
    this->stopEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->stopEventId) {
        printf("%s:%s epicsEventCreate failure for stop event\n", 
            driverName, functionName);
        return;
    }
    
    /* Allocate the raw buffer we use to read image files.  Only do this once */
    dims[0] = maxSizeX;
    dims[1] = maxSizeY;
    /* Allocate the raw buffer we use for flat fields. */
    this->pFlatField = this->pNDArrayPool->alloc(2, dims, NDUInt32, 0, NULL);
    
    /* Connect to camserver */
    status = pasynOctetSyncIO->connect(camserverPort, 0, &this->pasynUserCamserver, NULL);

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


    createParam(GermaniumDelayTimeString,      asynParamFloat64, &GermaniumDelayTime);
    createParam(GermaniumThresholdString,      asynParamFloat64, &GermaniumThreshold);
    createParam(GermaniumThresholdApplyString, asynParamInt32,   &GermaniumThresholdApply);
    createParam(GermaniumThresholdAutoApplyString, asynParamInt32,   &GermaniumThresholdAutoApply);
    createParam(GermaniumEnergyString,         asynParamFloat64, &GermaniumEnergy);
    createParam(GermaniumArmedString,          asynParamInt32,   &GermaniumArmed);
    createParam(GermaniumResetPowerString,     asynParamInt32,   &GermaniumResetPower);
    createParam(GermaniumResetPowerTimeString, asynParamInt32,   &GermaniumResetPowerTime);
    createParam(GermaniumImageFileTmotString,  asynParamFloat64, &GermaniumImageFileTmot);
    createParam(GermaniumBadPixelFileString,   asynParamOctet,   &GermaniumBadPixelFile);
    createParam(GermaniumNumBadPixelsString,   asynParamInt32,   &GermaniumNumBadPixels);
    createParam(GermaniumFlatFieldFileString,  asynParamOctet,   &GermaniumFlatFieldFile);
    createParam(GermaniumMinFlatFieldString,   asynParamInt32,   &GermaniumMinFlatField);
    createParam(GermaniumFlatFieldValidString, asynParamInt32,   &GermaniumFlatFieldValid);
    createParam(GermaniumGapFillString,        asynParamInt32,   &GermaniumGapFill);
    createParam(GermaniumWavelengthString,     asynParamFloat64, &GermaniumWavelength);
    createParam(GermaniumEnergyLowString,      asynParamFloat64, &GermaniumEnergyLow);
    createParam(GermaniumEnergyHighString,     asynParamFloat64, &GermaniumEnergyHigh);
    createParam(GermaniumDetDistString,        asynParamFloat64, &GermaniumDetDist);
    createParam(GermaniumDetVOffsetString,     asynParamFloat64, &GermaniumDetVOffset);
    createParam(GermaniumBeamXString,          asynParamFloat64, &GermaniumBeamX);
    createParam(GermaniumBeamYString,          asynParamFloat64, &GermaniumBeamY);
    createParam(GermaniumFluxString,           asynParamFloat64, &GermaniumFlux);
    createParam(GermaniumFilterTransmString,   asynParamFloat64, &GermaniumFilterTransm);
    createParam(GermaniumStartAngleString,     asynParamFloat64, &GermaniumStartAngle);
    createParam(GermaniumAngleIncrString,      asynParamFloat64, &GermaniumAngleIncr);
    createParam(GermaniumDet2thetaString,      asynParamFloat64, &GermaniumDet2theta);
    createParam(GermaniumPolarizationString,   asynParamFloat64, &GermaniumPolarization);
    createParam(GermaniumAlphaString,          asynParamFloat64, &GermaniumAlpha);
    createParam(GermaniumKappaString,          asynParamFloat64, &GermaniumKappa);
    createParam(GermaniumPhiString,            asynParamFloat64, &GermaniumPhi);
    createParam(GermaniumPhiIncrString,        asynParamFloat64, &GermaniumPhiIncr);
    createParam(GermaniumChiString,            asynParamFloat64, &GermaniumChi);
    createParam(GermaniumChiIncrString,        asynParamFloat64, &GermaniumChiIncr);
    createParam(GermaniumOmegaString,          asynParamFloat64, &GermaniumOmega);
    createParam(GermaniumOmegaIncrString,      asynParamFloat64, &GermaniumOmegaIncr);
    createParam(GermaniumOscillAxisString,     asynParamOctet,   &GermaniumOscillAxis);
    createParam(GermaniumNumOscillString,      asynParamInt32,   &GermaniumNumOscill);
    createParam(GermaniumPixelCutOffString,    asynParamInt32,   &GermaniumPixelCutOff);
    createParam(GermaniumThTemp0String,        asynParamFloat64, &GermaniumThTemp0);
    createParam(GermaniumThTemp1String,        asynParamFloat64, &GermaniumThTemp1);
    createParam(GermaniumThTemp2String,        asynParamFloat64, &GermaniumThTemp2);
    createParam(GermaniumThHumid0String,       asynParamFloat64, &GermaniumThHumid0);
    createParam(GermaniumThHumid1String,       asynParamFloat64, &GermaniumThHumid1);
    createParam(GermaniumThHumid2String,       asynParamFloat64, &GermaniumThHumid2);
    createParam(GermaniumTvxVersionString,     asynParamOctet,   &GermaniumTvxVersion);
    createParam(GermaniumCbfTemplateFileString,asynParamOctet,   &GermaniumCbfTemplateFile);
    createParam(GermaniumHeaderStringString,   asynParamOctet,   &GermaniumHeaderString);

    /* Set some default values for parameters */
    status =  setStringParam (ADManufacturer, "BNL");
    status |= setStringParam (ADModel, "Germanium");
    epicsSnprintf( versionString, sizeof(versionString), "%d.%d.%d", 
                   DRIVER_VERSION, DRIVER_REVISION, DRIVER_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);
    status |= setIntegerParam(ADMaxSizeX, maxSizeX);
    status |= setIntegerParam(ADMaxSizeY, maxSizeY);
    status |= setIntegerParam(ADSizeX, maxSizeX);
    status |= setIntegerParam(ADSizeX, maxSizeX);
    status |= setIntegerParam(ADSizeY, maxSizeY);
    status |= setIntegerParam(NDArraySizeX, maxSizeX);
    status |= setIntegerParam(NDArraySizeY, maxSizeY);
    status |= setIntegerParam(NDArraySize, 0);
    status |= setIntegerParam(NDDataType,  NDUInt32);
    status |= setIntegerParam(ADImageMode, ADImageContinuous);
    status |= setIntegerParam(ADTriggerMode, TMInternal);

    status |= setIntegerParam(GermaniumArmed, 0);
    status |= setIntegerParam(GermaniumResetPower, 0);
    status |= setIntegerParam(GermaniumResetPowerTime, 1);
    status |= setStringParam (GermaniumBadPixelFile, "");
    status |= setIntegerParam(GermaniumNumBadPixels, 0);
    status |= setStringParam (GermaniumFlatFieldFile, "");
    status |= setIntegerParam(GermaniumFlatFieldValid, 0);

    setDoubleParam(GermaniumThTemp0, 0);
    setDoubleParam(GermaniumThTemp1, 0);
    setDoubleParam(GermaniumThTemp2, 0);
    setDoubleParam(GermaniumThHumid0, 0);
    setDoubleParam(GermaniumThHumid1, 0);
    setDoubleParam(GermaniumThHumid2, 0);
    setStringParam(GermaniumTvxVersion, "Unknown");
    setStringParam(GermaniumHeaderString, "");

    if (status) {
        printf("%s: unable to set camera parameters\n", functionName);
        return;
    }
    
    /* Create the thread that updates the images */
    status = (epicsThreadCreate("GermaniumDetTask",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)germaniumTaskC,
                                this) == NULL);
    if (status) {
        printf("%s:%s epicsThreadCreate failure for image task\n", 
            driverName, functionName);
        return;
    }
    
    // Always call the germaniumStatus() function once to get TVX version, etc.
    // This must be done with the lock taken
    lock();
    germaniumStatus();
    unlock();

}

/* Code for iocsh registration */
static const iocshArg GermaniumDetectorConfigArg0 = {"Port name", iocshArgString};
static const iocshArg GermaniumDetectorConfigArg1 = {"camserver port name", iocshArgString};
static const iocshArg GermaniumDetectorConfigArg2 = {"maxSizeX", iocshArgInt};
static const iocshArg GermaniumDetectorConfigArg3 = {"maxSizeY", iocshArgInt};
static const iocshArg GermaniumDetectorConfigArg4 = {"maxBuffers", iocshArgInt};
static const iocshArg GermaniumDetectorConfigArg5 = {"maxMemory", iocshArgInt};
static const iocshArg GermaniumDetectorConfigArg6 = {"priority", iocshArgInt};
static const iocshArg GermaniumDetectorConfigArg7 = {"stackSize", iocshArgInt};
static const iocshArg * const GermaniumDetectorConfigArgs[] =  {&GermaniumDetectorConfigArg0,
                                                              &GermaniumDetectorConfigArg1,
                                                              &GermaniumDetectorConfigArg2,
                                                              &GermaniumDetectorConfigArg3,
                                                              &GermaniumDetectorConfigArg4,
                                                              &GermaniumDetectorConfigArg5,
                                                              &GermaniumDetectorConfigArg6,
                                                              &GermaniumDetectorConfigArg7};
static const iocshFuncDef configGermaniumDetector = {"GermaniumDetectorConfig", 8, GermaniumDetectorConfigArgs};
static void configGermaniumDetectorCallFunc(const iocshArgBuf *args)
{
    GermaniumDetectorConfig(args[0].sval, args[1].sval, args[2].ival,  args[3].ival,  
                          args[4].ival, args[5].ival, args[6].ival,  args[7].ival);
}


static void GermaniumDetectorRegister(void)
{

    iocshRegister(&configGermaniumDetector, configGermaniumDetectorCallFunc);
}

extern "C" {
epicsExportRegistrar(GermaniumDetectorRegister);
}

