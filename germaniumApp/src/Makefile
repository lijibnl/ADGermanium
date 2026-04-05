# Makefile for Germanium areaDetector driver (ZMQ version)

TOP=../..
include $(TOP)/configure/CONFIG

# Build the driver library
LIBRARY_IOC = germaniumDetector

DBD += germaniumDetectorSupport.dbd

# Source files
LIB_SRCS += germaniumDetector.cpp
LIB_SRCS += germaniumDetectorDriver.cpp
LIB_SRCS += germaniumDetectorZmq.cpp
LIB_SRCS += germaniumDetectorDataAcq.cpp
LIB_SRCS += germaniumDetectorApp.cpp

# ZMQ library
LIB_SYS_LIBS += zmq

include $(ADCORE)/ADApp/commonLibraryMakefile

include $(TOP)/configure/RULES
