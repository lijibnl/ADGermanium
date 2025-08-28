# Makefile for Germanium areaDetector driver

TOP=../..
include $(TOP)/configure/CONFIG

$(info ADCORE = $(ADCORE))

# Build the IOC application
LIBRARY_IOC = germanium

# GermaniumApp.dbd will be created and installed
DBD += germaniumDetectorSupport.dbd

# Germanium detector driver source files
LIB_SRCS += germaniumDetector.cpp
LIB_SRCS += germaniumDetector_network.cpp
 
#LIB_SRCS += Germanium.cpp
#LIB_SRCS += GermaniumDriver.cpp
#LIB_SRCS += GermaniumHardware.cpp
#LIB_SRCS += GermaniumNetwork.cpp
#LIB_SRCS += GermaniumDataAcq.cpp
#LIB_SRCS += GermaniumApp.cpp

include $(ADCORE)/ADApp/commonLibraryMakefile

include $(TOP)/configure/RULES
