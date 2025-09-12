
## 1. PV from ADBase

```epics
# replacing TP
record(ao, "$(P)$(R)AcquireTime")
{
   field(PINI, "YES")
   field(DTYP, "asynFloat64")
   field(OUT,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))ACQ_TIME")
   field(PREC, "3")
   field(VAL,  "1.0")
   info(autosaveFields, "VAL")
}

record(ai, "$(P)$(R)AcquireTime_RBV")
{
   field(DTYP, "asynFloat64")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))ACQ_TIME")
   field(PREC, "3")
   field(SCAN, "I/O Intr")
}

record(ai, "$(P)$(R)TimeRemaining_RBV")
{
   field(DTYP, "asynFloat64")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))TIME_REMAINING")
   field(PREC, "3")
   field(SCAN, "I/O Intr")
}

# Replacing GAIN
record(ao, "$(P)$(R)Gain")
{
   field(PINI, "YES")
   field(DTYP, "asynFloat64")
   field(OUT,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))GAIN")
   field(VAL,  "1.0")
   field(PREC, "3")
   info(autosaveFields, "VAL")
}

record(ai, "$(P)$(R)Gain_RBV")
{
   field(DTYP, "asynFloat64")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))GAIN")
   field(PREC, "3")
   field(SCAN, "I/O Intr")
}

## Maybe

record(ao, "$(P)$(R)AcquirePeriod")
{
   field(PINI, "YES")
   field(DTYP, "asynFloat64")
   field(OUT,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))ACQ_PERIOD")
   field(PREC, "3")
   field(VAL,  "0")
   info(autosaveFields, "VAL")
}

record(ai, "$(P)$(R)AcquirePeriod_RBV")
{
   field(DTYP, "asynFloat64")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))ACQ_PERIOD")
   field(PREC, "3")
   field(SCAN, "I/O Intr")
}


record(mbbo, "$(P)$(R)ImageMode")
{
   field(PINI, "YES")
   field(DTYP, "asynInt32")
   field(OUT,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))IMAGE_MODE")
   field(ZRST, "Single")
   field(ZRVL, "0")
   field(ONST, "Multiple")
   field(ONVL, "1")
   field(TWST, "Continuous")
   field(TWVL, "2")
   field(VAL,  "2")
   info(autosaveFields, "VAL")
}

record(mbbi, "$(P)$(R)ImageMode_RBV")
{
   field(DTYP, "asynInt32")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))IMAGE_MODE")
   field(ZRST, "Single")
   field(ZRVL, "0")
   field(ONST, "Multiple")
   field(ONVL, "1")
   field(TWST, "Continuous")
   field(TWVL, "2")
   field(SCAN, "I/O Intr")
}
```



## 2. PVs from NDArrayBase

```
record(stringin, "$(P)$(R)ADCoreVersion_RBV")
{
   field(DTYP, "asynOctetRead")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))ADCORE_VERSION")
   field(VAL,  "Unknown")
   field(SCAN, "I/O Intr")
}

record(stringin, "$(P)$(R)DriverVersion_RBV")
{
   field(DTYP, "asynOctetRead")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))DRIVER_VERSION")
   field(VAL,  "Unknown")
   field(SCAN, "I/O Intr")
}

record(stringin, "$(P)$(R)PortName_RBV")
{
   field(DTYP, "asynOctetRead")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))PORT_NAME_SELF")
   field(VAL,  "Unknown")
   field(SCAN, "I/O Intr")
}

record(stringin, "$(P)$(R)Manufacturer_RBV")
{
   field(DTYP, "asynOctetRead")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))MANUFACTURER")
   field(VAL,  "Unknown")
   field(SCAN, "I/O Intr")
}

record(stringin, "$(P)$(R)Model_RBV")
{
   field(DTYP, "asynOctetRead")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))MODEL")
   field(VAL,  "Unknown")
   field(SCAN, "I/O Intr")
}

record(stringin, "$(P)$(R)SerialNumber_RBV")
{
   field(DTYP, "asynOctetRead")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))SERIAL_NUMBER")
   field(VAL,  "Unknown")
   field(SCAN, "I/O Intr")
}

record(stringin, "$(P)$(R)FirmwareVersion_RBV")
{
   field(DTYP, "asynOctetRead")
   field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))FIRMWARE_VERSION")
   field(VAL,  "Unknown")
   field(SCAN, "I/O Intr")
}

record(bo, "$(P)$(R)Acquire") {
   field(DTYP, "asynInt32")
   field(OUT, "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))ACQUIRE")
   field(ZNAM, "Done")
   field(ONAM, "Acquire")
   field(VAL,  "0")
   field(FLNK, "$(P)$(R)SetAcquireBusy")
   info(asyn:READBACK, "1")
}

record(bi, "$(P)$(R)Acquire_RBV") {
   field(DTYP, "asynInt32")
   field(INP, "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))ACQUIRE")
   field(ZNAM, "Done")
   field(ZSV,  "NO_ALARM")
   field(ONAM, "Acquiring")
   field(OSV,  "MINOR")
   field(SCAN, "I/O Intr")
}

record(ai, "$(P)$(R)TimeStamp_RBV")
{
    field(DTYP, "asynFloat64")
    field(INP,  "@asyn($(PORT),$(ADDR=0),$(TIMEOUT=1))TIME_STAMP")
    field(PREC, "3")
    field(SCAN, "$(SCANRATE=I/O Intr)")
}
```

