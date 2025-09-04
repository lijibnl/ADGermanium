#!../../bin/linux-x86_64/germaniumDetector


< envPaths
errlogInit(20000)

dbLoadDatabase("$(TOP)/dbd/germaniumDetector.dbd")
germaniumDetector_registerRecordDeviceDriver(pdbbase) 

epicsEnvSet("PREFIX", "Det")
# The port name for the detector
epicsEnvSet("PORT",   "GERM")
epicsEnvSet("QSIZE",  "20")
# The maximim image width; used for row profiles in the NDPluginStats plugin
epicsEnvSet("XSIZE",  "487")
# The maximim image height; used for column profiles in the NDPluginStats plugin
epicsEnvSet("YSIZE",  "195")
# The maximum number of time seried points in the NDPluginStats plugin
epicsEnvSet("NCHANS", "2048")
# The maximum number of frames buffered in the NDPluginCircularBuff plugin
epicsEnvSet("CBUFFS", "500")
# The search path for database files
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

epicsEnvSet("Detector_IP", "172.16.0.211")
epicsEnvSet("Detector_PORT", "9527")
epicsEnvSet("NELM", "384")

#drvAsynIPPortConfigure("GeRM","$(Detector_IP):$(Detector_PORT)")
#asynOctetSetInputEos("camserver", 0, "\x18")
#asynOctetSetOutputEos("camserver", 0, "\n")
echo "NELM=$(NELM)"

germaniumConfig( "$(PORT)", "$(Detector_IP)", $(NELM), 2, 512, 100, 10485760 )
asynReport 5, "$(PORT)"

dbLoadRecords( "$(ADGERMANIUM)/db/Germanium.template", "P=$(PREFIX), R=, PORT=GERM, ADDR=0" )
#
## Create a standard arrays plugin
#NDStdArraysConfigure( "Image1", 5, 0, "$(PORT)", 0, 0 )
#dbLoadRecords( "$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),TYPE=Int32,FTVL=LONG,NELEMENTS=94965" )


# Load all other plugins using commonPlugins.cmd
#< $(ADCORE)/iocBoot/commonPlugins.cmd
#set_requestfile_path("$(ADGERMANIUM)/germaniumApp/Db")


cd "${TOP}/iocBoot/${IOC}"
iocInit

# save things every thirty seconds
#create_monitor_set("auto_settings.req", 30,"P=$(PREFIX)")

