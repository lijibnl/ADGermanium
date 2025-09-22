#!../../bin/linux-x86_64/germaniumDetector


< envPaths
< lab.uniq
errlogInit(20000)

dbLoadDatabase("$(TOP)/dbd/germaniumDetector.dbd")
germaniumDetector_registerRecordDeviceDriver(pdbbase) 

# The port name for the detector
epicsEnvSet("QSIZE",  "20")

epicsEnvSet("DETECTOR_PORT", "GERM")
epicsEnvSet("MCA_PORT",      "ARR_MCA")
epicsEnvSet("TDC_PORT",      "ARR_TDC")
epicsEnvSet("SPCT_PORT",     "ARR_SPCT")
epicsEnvSet("INTENS_PORT",   "ARR_INTENS")

epicsEnvSet("DETECTOR_ADDR", "0")
epicsEnvSet("MCA_ADDR",      "1")
epicsEnvSet("TDC_ADDR",      "2")
epicsEnvSet("SPCT_ADDR",     "3")
epicsEnvSet("INTENS_ADDR",   "4")

epicsEnvSet("", "")
epicsEnvSet("", "")
epicsEnvSet("", "")
epicsEnvSet("", "")
epicsEnvSet("", "")
epicsEnvSet("", "")
epicsEnvSet("", "")
epicsEnvSet("", "")

epicsEnvSet("MCA_XSIZE",    "4096")
epicsEnvSet("MCA_YSIZE",    "$(NELM)")
epicsEnvSet("TDC_XSIZE",    "1024")
epicsEnvSet("TDC_YSIZE",    "$(NELM)")
epicsEnvSet("SPCT_XSIZE",   "$(MCA_XSIZE")
epicsEnvSet("INTENS_XSIZE", "$(NELM"")
# The maximum number of time seried points in the NDPluginStats plugin
epicsEnvSet("NCHANS", "2048")
# The maximum number of frames buffered in the NDPluginCircularBuff plugin
epicsEnvSet("CBUFFS", "500")
# The search path for database files
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

epicsEnvSet("Detector_IP", "172.16.0.211")
epicsEnvSet("NELM", "384")

echo "NELM=$(NELM)"

germaniumConfig( "$(PORT)"
               , "$(Detector_IP)"
               , $(NELM)
               , 2
               , 512
               , 100
               , 10485760
               , $(MCA_ADDR)
               , $(TDC_ADDR)
               , $(SPCT_ADDR)
               , $(INTENS_ADDR)
               )
asynReport 5, "$(PORT)"

dbLoadRecords( "$(ADGERMANIUM)/db/germanium.db", "P=$(PREFIX), R=, PORT=GERM, ADDR=0" )
#
## Create a standard arrays plugin
#NDStdArraysConfigure( "Image1", 5, 0, "$(PORT)", 0, 0 )
#dbLoadRecords( "$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),TYPE=Int32,FTVL=LONG,NELEMENTS=94965" )

NDStdArraysConfigure("MCA",    20, 0, "GERM", $(MCA_ADDR),    0, 0, 0, 0)
NDStdArraysConfigure("TDC",    20, 0, "GERM", $(TDC_ADDR),    0, 0, 0, 0)
NDStdArraysConfigure("SPCT",   20, 0, "GERM", $(SPCT_ADDR),   0, 0, 0, 0)
NDStdArraysConfigure("INTENS", 20, 0, "GERM", $(INTENS_ADDR), 0, 0, 0, 0)

dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX), R=MCA:,    PORT=$(MCA_PORT),    ADDR=0, NDARRAY_PORT=$(DETECTOR_PORT)")
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX), R=TDC:,    PORT=$(TDC_PORT),    ADDR=0, NDARRAY_PORT=$(DETECTOR_PORT)")
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX), R=SPCT:,   PORT=$(SPCT_PORT),   ADDR=0, NDARRAY_PORT=$(DETECTOR_PORT)")
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX), R=INTENS:, PORT=$(INTENS_PORT), ADDR=0, NDARRAY_PORT=$(DETECTOR_PORT)")


# Load all other plugins using commonPlugins.cmd
#< $(ADCORE)/iocBoot/commonPlugins.cmd
#set_requestfile_path("$(ADGERMANIUM)/germaniumApp/Db")


cd "${TOP}/iocBoot/${IOC}"
iocInit

# save things every thirty seconds
#create_monitor_set("auto_settings.req", 30,"P=$(PREFIX)")

