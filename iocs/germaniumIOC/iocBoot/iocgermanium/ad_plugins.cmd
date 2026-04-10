# Standard areaDetector plugins
epicsEnvSet("QSIZE",  "20")
epicsEnvSet("XSIZE",  "4096")
epicsEnvSet("YSIZE",  "$(NELM)")
epicsEnvSet("NCHANS", "2048")
epicsEnvSet("CBUFFS", "500")
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

# NDStdArrays: waveform access to array data via CA/PVA
# MCA array plugin (addr 0) — NELEMENTS = 4096 * NELM
NDStdArraysConfigure("MCA1", $(QSIZE), 0, "$(PORT)", 0, 0)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=MCA1:,PORT=MCA1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0,TYPE=Int32,FTVL=LONG,NELEMENTS=$(MCA_NELM)")

# TDC array plugin (addr 1) — NELEMENTS = 1024 * NELM
NDStdArraysConfigure("TDC1", $(QSIZE), 0, "$(PORT)", 1, 0)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=TDC1:,PORT=TDC1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=1,TYPE=Int32,FTVL=LONG,NELEMENTS=$(TDC_NELM)")

# HDF5 file plugin for MCA data
NDFileHDF5Configure("McaHDF1", $(QSIZE), 0, "$(PORT)", 0)
dbLoadRecords("$(ADCORE)/db/NDFileHDF5.template", "P=$(PREFIX),R=McaHDF1:,PORT=McaHDF1,ADDR=0,TIMEOUT=1,XMLSIZE=2048,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")

# HDF5 file plugin for TDC data
NDFileHDF5Configure("TdcHDF1", $(QSIZE), 0, "$(PORT)", 1)
dbLoadRecords("$(ADCORE)/db/NDFileHDF5.template", "P=$(PREFIX),R=TdcHDF1:,PORT=TdcHDF1,ADDR=0,TIMEOUT=1,XMLSIZE=2048,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=1")

# Stats plugin for MCA
NDStatsConfigure("STATS1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, 5)
NDTimeSeriesConfigure("STATS1_TS", $(QSIZE), 0, "STATS1", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDStats.template", "P=$(PREFIX),R=Stats1:,PORT=STATS1,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")

