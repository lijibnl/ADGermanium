
/**
 * @file EpicsPoller.hpp
 * @brief Header file for the EpicsPoller class for periodic polling of EPICS PVs.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * 
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

 #pragma once

//===========================================================================//

#include <vector>
#include <memory>
#include <functional>
#include <epicsThread.h>

//===========================================================================//

class EpicsPollItem
{
public:
    using PollFunc = std::function<void()>;

    EpicsPollItem( int slowDivider
                 , int fastDivider
                 , PollFunc pollFunc
                 );
;
    int divider_;
    int dividerSlow_;
    int dividerFast_;

    void execute();

private:
    PollFunc pollFunc_;
};

//=============================================================================//

// ─────────────────────────────────────────────
// Poller class
// ─────────────────────────────────────────────
class EpicsPoller {
public:
    EpicsPoller( double basePeriod = 0.1 );
    ~EpicsPoller();

    void addItem(std::unique_ptr<EpicsPollItem> item);
    void setFast( bool fast );
    void setRunning( bool running );

private:
    std::vector<std::unique_ptr<EpicsPollItem>> pollItems_;

    std::atomic<bool> pollItemFast_; // parallel vector to track which items are "fast"
    
    double basePeriod_;
    int    tick_;

    epicsThreadId threadId_;
    std::atomic<bool> running_;

    static void threadFuncC(void *p);
    void threadFunc();
};

