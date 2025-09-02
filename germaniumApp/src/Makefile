# Makefile for Germanium areaDetector driver

TOP=../..
include $(TOP)/configure/CONFIG

$(info ADCORE = $(ADCORE))

# Build the IOC application
LIBRARY_IOC = germaniumDetector

# GermaniumApp.dbd will be created and installed
DBD += germaniumDetectorSupport.dbd

# Germanium detector driver source files
#LIB_SRCS += germaniumDetector.cpp
#LIB_SRCS += germaniumDetector_network.cpp
 
LIB_SRCS += germaniumDetector.cpp
LIB_SRCS += germaniumDetectorDriver.cpp
LIB_SRCS += germaniumDetectorHardware.cpp
LIB_SRCS += germaniumDetectorNetwork.cpp
LIB_SRCS += germaniumDetectorDataAcq.cpp
LIB_SRCS += germaniumDetectorApp.cpp

include $(ADCORE)/ADApp/commonLibraryMakefile

include $(TOP)/configure/RULES
