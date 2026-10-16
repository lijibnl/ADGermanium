#!../../bin/linux-x86_64/germaniumDetector

< envPaths
errlogInit(20000)

##=====================================================##

dbLoadDatabase("$(TOP)/dbd/germaniumDetector.dbd")
germaniumDetector_registerRecordDeviceDriver(pdbbase)

##=====================================================##

# Load environment specific configurations
< unique.cmd

##=====================================================##

# Create the Germanium detector driver
# germaniumConfig(portName, numElements, ipAddress, maxAddr, numParams, maxBuffers, maxMemory)
# maxAddr=2: addr 0 = MCA (4096 × NELM), addr 1 = TDC (1024 × NELM)
germaniumConfig("$(PORT)", $(NELM), "$(ZYNQ_MAN_IP)", 2, 0, 50, 0)

# Load detector PV records
dbLoadRecords("$(ADGERMANIUM)/db/Germanium.template", "P=$(PREFIX),R=,PORT=$(PORT),ADDR=0,NELM=$(NELM),MCA_NELM=$(MCA_NELM),TDC_NELM=$(TDC_NELM)")

##=====================================================##

# Load asyn records
dbLoadRecords("$(ASYN)/db/asynRecord.db", "P=$(PREFIX),R=asyn1,PORT=$(PORT),ADDR=0,OMAX=0,IMAX=0")

##=====================================================##

# Load areaDetector plugins and records
< ad_plugins.cmd

##=====================================================##

# Set path for auto-saving/restoring settings
#set_requestfile_path("$(ADGERMANIUM)/germaniumApp/Db")

##=====================================================##

# Start the IOC
cd "${TOP}/iocBoot/${IOC}"
iocInit

##=====================================================##

# Save/restore
#create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

##=====================================================##

dbl > pv.list
#dbl

##=====================================================##

#dbpf $(PREFIX)asyn1.TMSK 0x3f
#dbpf $(PREFIX)asyn1.TIOM 0x7
#dbpf $(PREFIX)asyn1.TINM 0xf

dbpf $(PREFIX)IPAddress $(ZYNQ_DATA_IP)

# Enable NDArrays
dbpf $(PREFIX)MCA1:EnableCallbacks 1
dbpf $(PREFIX)MCA1:ArrayCallbacks 1
dbpf $(PREFIX)MCA1:Acquire 1
dbpf $(PREFIX)TDC1:EnableCallbacks 1
dbpf $(PREFIX)TDC1:ArrayCallbacks 1
dbpf $(PREFIX)TDC1:Acquire 1

< test.cmd

