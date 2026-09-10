##=============================================================
# Standard areaDetector plugins
epicsEnvSet("QSIZE",  "20")
epicsEnvSet("XSIZE",  "4096")
epicsEnvSet("YSIZE",  "$(NELM)")
epicsEnvSet("NCHANS", "2048")
#epicsEnvSet("CBUFFS", "500")
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")
##----------------------------------------------------------
## MCA

# NDStdArrays
# MCA array plugin (addr 0) — NELEMENTS = 4096 * NELM
NDStdArraysConfigure("Image1", $(QSIZE), 0, "$(PORT)", 0, 0)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=MCA1:image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0,TYPE=Int32,FTVL=LONG,NELEMENTS=$(MCA_NELM)")

# NDPluginPva: expose MCA1 NDArray as an NTNDArray PV
NDPvaConfigure("MCA1:PVA1", $(QSIZE), 0, "$(PORT)", 0, "$(PREFIX)MCA1:PVA1:Image", 0, 0, 0, 0)

dbLoadRecords("$(ADCORE)/db/NDPva.template", \
    "P=$(PREFIX),R=MCA1:Pva1:,PORT=MCA1:PVA1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")

startPVAServer

# NDPluginProcess
NDProcessConfigure("MCA1:PROC1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDProcess.template", \
    "P=$(PREFIX),R=MCA1:Proc1:,PORT=MCA1:PROC1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")

# NDPluginTransform
#NDTransformConfigure("MCA1:TRANS1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0)
#dbLoadRecords("$(ADCORE)/db/NDTransform.template", \
#    "P=$(PREFIX)MCA1:,R=Transform1:,PORT=MCA1:TRANS1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")

NDTransformConfigure("MCA1:TRANS1", 20, 0, "GERM", 0, 0, 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDTransform.template", "P=$(PREFIX),R=MCA1:Trans1:,PORT=MCA1:TRANS1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=GERM,NDARRAY_ADDR=0")

# ROI1-4
NDROIConfigure("MCA1:ROI1", 20, 0, "GERM", 0, 0, 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDROI.template", "P=$(PREFIX),R=MCA1:ROI1:,PORT=MCA1:ROI1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=GERM,NDARRAY_ADDR=0")

NDROIConfigure("MCA1:ROI2", 20, 0, "GERM", 0, 0, 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDROI.template", "P=$(PREFIX),R=MCA1:ROI2:,PORT=MCA1:ROI2,ADDR=0,TIMEOUT=1,NDARRAY_PORT=GERM,NDARRAY_ADDR=0")

NDROIConfigure("MCA1:ROI3", 20, 0, "GERM", 0, 0, 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDROI.template", "P=$(PREFIX),R=MCA1:ROI3:,PORT=MCA1:ROI3,ADDR=0,TIMEOUT=1,NDARRAY_PORT=GERM,NDARRAY_ADDR=0")

NDROIConfigure("MCA1:ROI4", 20, 0, "GERM", 0, 0, 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDROI.template", "P=$(PREFIX),R=MCA1:ROI4:,PORT=MCA1:ROI4,ADDR=0,TIMEOUT=1,NDARRAY_PORT=GERM,NDARRAY_ADDR=0")



# Stats1-5
# Stats1 - full MCA NDArray

# _NDStatsConfigure("MCA1:STATS1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
# _dbLoadRecords("$(ADCORE)/db/NDStats.template", "P=$(PREFIX),R=MCA1:Stats1:,PORT=MCA1:STATS1,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=$(PORT)")


# _NDTimeSeriesConfigure("MCA0:STATS1:TS", $(QSIZE), 0, "MCA1:STATS1", 1, 23)
# _dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template", "P=$(PREFIX),R=MCA1:Stats1TS:,PORT=MCA1:STATS1:TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=MCA1:STATS1,NDARRAY_ADDR=1,NCHANS=2048,ENABLED=1")

# Stats1
NDStatsConfigure("MCA1:STATS1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("$(ADCORE)/db/NDStats.template", "P=$(PREFIX),R=MCA1:Stats1:,PORT=MCA1:STATS1,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=$(PORT)")
NDTimeSeriesConfigure("MCA1:STATS1_TS", $(QSIZE), 0, "MCA1:STATS1", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template", "P=$(PREFIX),R=MCA1:Stats1TS:,PORT=MCA1:STATS1_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=MCA1:STATS1,NDARRAY_ADDR=1,NCHANS=2048,ENABLED=1")

# Stats2
NDStatsConfigure("MCA1:STATS2", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("$(ADCORE)/db/NDStats.template", "P=$(PREFIX),R=MCA1:Stats2:,PORT=MCA1:STATS2,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=$(PORT)")
NDTimeSeriesConfigure("MCA1:STATS2_TS", $(QSIZE), 0, "MCA1:STATS2", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template", "P=$(PREFIX),R=MCA1:Stats2TS:,PORT=MCA1:STATS2_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=MCA1:STATS2,NDARRAY_ADDR=1,NCHANS=2048,ENABLED=1")

# Stats3
NDStatsConfigure("MCA1:STATS3", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("$(ADCORE)/db/NDStats.template", "P=$(PREFIX),R=MCA1:Stats3:,PORT=MCA1:STATS3,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=$(PORT)")
NDTimeSeriesConfigure("MCA1:STATS3_TS", $(QSIZE), 0, "MCA1:STATS3", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template", "P=$(PREFIX),R=MCA1:Stats3TS:,PORT=MCA1:STATS3_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=MCA1:STATS3,NDARRAY_ADDR=1,NCHANS=2048,ENABLED=1")

# Stats4
NDStatsConfigure("MCA1:STATS4", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("$(ADCORE)/db/NDStats.template", "P=$(PREFIX),R=MCA1:Stats4:,PORT=MCA1:STATS4,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=$(PORT)")
NDTimeSeriesConfigure("MCA1:STATS4_TS", $(QSIZE), 0, "MCA1:STATS4", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template", "P=$(PREFIX),R=MCA1:Stats4TS:,PORT=MCA1:STATS4_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=MCA1:STATS4,NDARRAY_ADDR=1,NCHANS=2048,ENABLED=1")

# Stats5
NDStatsConfigure("MCA1:STATS5", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("$(ADCORE)/db/NDStats.template", "P=$(PREFIX),R=MCA1:Stats5:,PORT=MCA1:STATS5,ADDR=0,TIMEOUT=1,HIST_SIZE=256,XSIZE=$(XSIZE),YSIZE=$(YSIZE),NCHANS=$(NCHANS),NDARRAY_PORT=$(PORT)")
NDTimeSeriesConfigure("MCA1:STATS5_TS", $(QSIZE), 0, "MCA1:STATS5", 1, 23)
dbLoadRecords("$(ADCORE)/db/NDTimeSeries.template", "P=$(PREFIX),R=MCA1:Stats5TS:,PORT=MCA1:STATS5_TS,ADDR=0,TIMEOUT=1,NDARRAY_PORT=MCA1:STATS5,NDARRAY_ADDR=1,NCHANS=2048,ENABLED=1")

# HDF5 file plugin for MCA data
NDFileHDF5Configure("MCA1:HDF1", $(QSIZE), 0, "$(PORT)", 0)
dbLoadRecords("$(ADCORE)/db/NDFileHDF5.template", "P=$(PREFIX),R=MCA1:HDF1:,PORT=MCA1:HDF1,ADDR=0,TIMEOUT=1,XMLSIZE=2048,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")


# TIFF file plugin
NDFileTIFFConfigure("MCA1:TIFF1", $(QSIZE), 0, "$(PORT)", 0)
dbLoadRecords("$(ADCORE)/db/NDFileTIFF.template", \
    "P=$(PREFIX),R=MCA1:TIFF1:,PORT=MCA1:TIFF1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")

# NDFileJPEG
NDFileJPEGConfigure("MCA1:JPEG1", $(QSIZE), 0, "$(PORT)", 0)
dbLoadRecords("$(ADCORE)/db/NDFileJPEG.template", "P=$(PREFIX),R=MCA1:JPEG1:,PORT=MCA1:JPEG1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")

# ROIStat1
NDROIStatConfigure("MCA1:ROISTAT1", $(QSIZE), 0, "$(PORT)", 0, 4, 0, 0, 0, 0, $(MAX_THREADS=5))
dbLoadRecords("$(ADCORE)/db/NDROIStat.template", "P=$(PREFIX),R=MCA1:ROIStat1:,PORT=MCA1:ROISTAT1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0,NCHANS=$(NCHANS)")
dbLoadRecords("$(ADCORE)/db/NDROIStatN.template", "P=$(PREFIX),R=MCA1:ROIStat1:1:,PORT=MCA1:ROISTAT1,ADDR=0,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("$(ADCORE)/db/NDROIStatN.template", "P=$(PREFIX),R=MCA1:ROIStat1:2:,PORT=MCA1:ROISTAT1,ADDR=1,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("$(ADCORE)/db/NDROIStatN.template", "P=$(PREFIX),R=MCA1:ROIStat1:3:,PORT=MCA1:ROISTAT1,ADDR=2,TIMEOUT=1,NCHANS=$(NCHANS)")
dbLoadRecords("$(ADCORE)/db/NDROIStatN.template", "P=$(PREFIX),R=MCA1:ROIStat1:4:,PORT=MCA1:ROISTAT1,ADDR=3,TIMEOUT=1,NCHANS=$(NCHANS)")

##----------------------------------------------------------
## TDC

# TDC array plugin (addr 1) — NELEMENTS = 1024 * NELM
NDStdArraysConfigure("TDC1", $(QSIZE), 0, "$(PORT)", 1, 0)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=TDC1:,PORT=TDC1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=1,TYPE=Int32,FTVL=LONG,NELEMENTS=$(TDC_NELM)")

# HDF5 file plugin for TDC data
NDFileHDF5Configure("TdcHDF1", $(QSIZE), 0, "$(PORT)", 1)
dbLoadRecords("$(ADCORE)/db/NDFileHDF5.template", "P=$(PREFIX),R=TdcHDF1:,PORT=TdcHDF1,ADDR=0,TIMEOUT=1,XMLSIZE=2048,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=1")


##=============================================================
