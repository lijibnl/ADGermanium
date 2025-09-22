TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
PROD_NAME = germaniumDetector

#=============================
# Build the IOC application

PROD_IOC = $(PROD_NAME)

DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += $(ADGERMANIUM)/dbd/germaniumDetectorSupport.dbd

# germanium.dbd will be made up from these files:
PROD_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp
PROD_SRCS += $(PROD_NAME)Main.cpp


# Add all the support libraries needed by this IOC
PROD_LIBS += germaniumDetector

# germanium_registerRecordDeviceDriver.cpp derives from germanium.dbd
#germanium_SRCS += germanium_registerRecordDeviceDriver.cpp

include $(ADCORE)/ADApp/commonDriverMakefile
$(PROD_NAME)_DBD += drvAsynIPPort.dbd


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

