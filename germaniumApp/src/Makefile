# Makefile for Germanium areaDetector driver (ZMQ version)

TOP=../..
include $(TOP)/configure/CONFIG

# Build the driver library
LIBRARY_IOC = GermaniumDetector

DBD += GermaniumDetectorSupport.dbd

# Source files
LIB_SRCS += GermaniumDetector.cpp
LIB_SRCS += GermaniumDetectorDriver.cpp
#LIB_SRCS += GermaniumDetectorZmq.cpp
LIB_SRCS += GermaniumZmq.cpp
LIB_SRCS += GermaniumDetectorDataAcq.cpp
LIB_SRCS += GermaniumDetectorParamFormat.cpp
LIB_SRCS += GermaniumDetectorApp.cpp
LIB_SRCS += EpicsPoller.cpp

LIBS += asyn
LIB_SYS_LIBS += zmq

USR_CXXFLAGS += -std=c++20

include $(ADCORE)/ADApp/commonLibraryMakefile

include $(TOP)/configure/RULES
