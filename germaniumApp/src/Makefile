# Makefile for Germanium areaDetector driver (ZMQ version)

TOP=../..
include $(TOP)/configure/CONFIG

# Build the driver library
LIBRARY_IOC = GermaniumDetector

GERMANIUM_PROTOCOL ?= $(TOP)/../../prj/germanium/protocol
USR_INCLUDES += -I$(GERMANIUM_PROTOCOL)

DBD += GermaniumDetectorSupport.dbd

# Source files
LIB_SRCS += GermaniumDetector.cpp
LIB_SRCS += GermaniumDetectorDriver.cpp
LIB_SRCS += GermaniumDetectorZmq.cpp
LIB_SRCS += GermaniumDetectorDataAcq.cpp
LIB_SRCS += GermaniumDetectorUdpWatchdog.cpp
LIB_SRCS += GermaniumDetectorParamFormat.cpp
LIB_SRCS += GermaniumDetectorApp.cpp
LIB_SRCS += EpicsPoller.cpp
LIB_SRCS += Zmq.cpp

LIBS += asyn

#========================================================
# TEMPORARY FIX
# The following line is commented to use hand compiled lib
#LIB_SYS_LIBS += zmq

CCC = /nsls2/users/liji/local/gcc-12.5/bin/g++
USR_CXXFLAGS += -std=c++20

#========================================================
# TEMPORARY FIX
# The following lines are added to use hand compiled.
# RHEL8 use old library which doesn't support .set()
# used in Zmq.cpp
USR_CXXFLAGS += -I$(HOME)/local/include
USR_CXXFLAGS += -I$(HOME)/local/zeromq/include
USR_LDFLAGS += -L$(HOME)/local/zeromq/lib64
USR_LDFLAGS += -Wl,-rpath,$(HOME)/local/zeromq/lib64
USR_LDFLAGS += -lzmq
#USR_LIBS += zmq
#========================================================

include $(ADCORE)/ADApp/commonLibraryMakefile

include $(TOP)/configure/RULES
