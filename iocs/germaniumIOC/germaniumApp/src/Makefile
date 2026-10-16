TOP=../..

include $(TOP)/configure/CONFIG

PROD_NAME = germaniumDetector

PROD_IOC = $(PROD_NAME)

DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += base.dbd
$(PROD_NAME)_DBD += GermaniumDetectorSupport.dbd
$(PROD_NAME)_DBD += asyn.dbd
$(PROD_NAME)_DBD += busySupport.dbd
$(PROD_NAME)_DBD += calcSupport.dbd


PROD_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp
PROD_SRCS += $(PROD_NAME)Main.cpp

PROD_LIBS += GermaniumDetector
PROD_LIBS += busy

# ZMQ system library
PROD_SYS_LIBS += zmq

include $(ADCORE)/ADApp/commonDriverMakefile

include $(TOP)/configure/RULES
