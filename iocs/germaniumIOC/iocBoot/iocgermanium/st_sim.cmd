#!../../bin/linux-x86_64/germaniumDetector

< envPaths
errlogInit(20000)

dbLoadDatabase("$(TOP)/dbd/germaniumDetector.dbd")
germaniumDetector_registerRecordDeviceDriver(pdbbase)

# PV prefix and asyn port
epicsEnvSet("PREFIX", "XF:28IDC-ES:1{Det:GeRM1}")
epicsEnvSet("PORT",   "GERM")

# Detector configuration
epicsEnvSet("NELM",   "192")       # Number of elements (96, 192, or 384)

# Zynq IP address (PS network interface where ZMQ server runs)
epicsEnvSet("ZYNQ_IP", "127.0.0.1")

# Create the Germanium detector driver
# germaniumConfig(portName, numElements, ipAddress, maxAddr, numParams, maxBuffers, maxMemory)
germaniumConfig("$(PORT)", $(NELM), "$(ZYNQ_IP)", 0, 0, 50, 0)

# Load detector PV records
# MCA_NELM = NELM * 4096, TDC_NELM = NELM * 1024
dbLoadRecords("$(ADGERMANIUM)/db/Germanium.template", "P=$(PREFIX),R=,PORT=$(PORT),ADDR=0,NELM=$(NELM),MCA_NELM=786432,TDC_NELM=196608")

# Standard areaDetector plugins
epicsEnvSet("QSIZE",  "20")
epicsEnvSet("XSIZE",  "4096")
epicsEnvSet("YSIZE",  "$(NELM)")
epicsEnvSet("NCHANS", "2048")
epicsEnvSet("CBUFFS", "500")
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

NDStdArraysConfigure("Image1", 5, 0, "$(PORT)", 0, 0)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),TYPE=Int32,FTVL=LONG,NELEMENTS=786432")

< $(ADCORE)/iocBoot/commonPlugins.cmd
set_requestfile_path("$(ADGERMANIUM)/germaniumApp/Db")

cd "${TOP}/iocBoot/${IOC}"
iocInit

# Save/restore
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")
