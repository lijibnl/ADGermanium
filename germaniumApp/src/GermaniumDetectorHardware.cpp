/**
 * @file GermaniumDetectorHardware.cpp
 * @brief MARS ASIC configuration initialization.
 *
 * With the delta-config ZMQ protocol, the Zynq server maintains its own
 * globalstr[]/channelstr[] and performs packing + register loading locally.
 * The IOC only sends field-level updates (SET_GLOBAL, SET_CHANNEL) and
 * a LOAD trigger. This file contains only the local shadow initialization.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * 
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "GermaniumDetector.hpp"
#include <cstdio>

//===========================================================================//

/*
 * Initialize the local shadow copies of MARS ASIC configuration.
 * These are used for PV readback and to track what the IOC has sent
 * to the Zynq. The Zynq maintains its own authoritative copy.
 */
void GermaniumDetector::initializeMarsConfig()
{
    for (int chip = 0; chip < nchips; chip++)
    {
        globalstr[chip].st   = 1;    // 0.25us
        globalstr[chip].g    = 0;    // 240keV
        globalstr[chip].pol  = 1;    // Positive
        globalstr[chip].eblk = 1;    // 2pA
        globalstr[chip].gmon = 0;    // Off
        globalstr[chip].puen = 0;    // Disabled
        globalstr[chip].mfs  = 0;    // Off
        globalstr[chip].tds  = 0;    // 1us
        globalstr[chip].tdm  = 0;    // ToA
        globalstr[chip].th   = 512;  // Default threshold
        globalstr[chip].c    = 0;
        globalstr[chip].m0   = 0;
        globalstr[chip].saux = 0;
    }

    for (int ch = 0; ch < numElements; ch++)
    {
        channelstr[ch].chen = 1;
        channelstr[ch].tsen = 0;
        channelstr[ch].thtr = 0;
        channelstr[ch].putr = 0;
    }

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s: MARS config shadow initialized for %d chips, %d elements\n", portName, nchips, numElements);
}

//===========================================================================//
