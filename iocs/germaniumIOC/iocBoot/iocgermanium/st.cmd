#!../../bin/linux-x86_64/germaniumDetector

< envPaths
errlogInit(20000)

##=====================================================##

dbLoadDatabase("$(TOP)/dbd/germaniumDetector.dbd")
germaniumDetector_registerRecordDeviceDriver(pdbbase)

##=====================================================##

# Load environment specific configurations
< env.lab

##=====================================================##

# Create the Germanium detector driver
# germaniumConfig(portName, numElements, ipAddress, maxAddr, numParams, maxBuffers, maxMemory)
# maxAddr=2: addr 0 = MCA (4096 × NELM), addr 1 = TDC (1024 × NELM)
germaniumConfig("$(PORT)", $(NELM), "$(ZYNQ_IP)", 2, 0, 50, 0)

# Load detector PV records
dbLoadRecords("$(ADGERMANIUM)/db/Germanium.template",
              "P=$(PREFIX),R=,PORT=$(PORT),ADDR=0,NELM=$(NELM),MCA_NELM=$(MCA_NELM),TDC_NELM=$(TDC_NELM)")

##=====================================================##
# Load asyn records

dbLoadRecords("$(ASYN)/db/asynRecord.db",
              "P=$(PREFIX),R=asyn1,PORT=$(PORT),ADDR=0,OMAX=0,IMAX=0")

##=====================================================##
# Load areaDetector plugins and records

< ad_plugins.cmd

##=====================================================##
# Set path for auto-saving/restoring settings

set_requestfile_path("$(ADGERMANIUM)/germaniumApp/Db")

##=====================================================##
# Start the IOC

cd "${TOP}/iocBoot/${IOC}"
iocInit

##=====================================================##
# Save/restore

create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

##=====================================================##

dbl > pv.list

##=====================================================##
