/**
 * @file EpicsPoller.cpp
 * @brief Implementation of the EpicsPoller class for periodic polling of EPICS PVs.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * 
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include <atomic>
#include <stdio.h>
//#include <print>
#include <iostream>

#include "EpicsPoller.hpp"

//===========================================================================//

EpicsPollItem::EpicsPollItem( int slowDivider, int fastDivider, PollFunc pollFunc )
                            : divider_    ( slowDivider )
                            , dividerSlow_( slowDivider )
                            , dividerFast_( fastDivider )
                            , pollFunc_   ( pollFunc    )
{}

//===========================================================================//

void EpicsPollItem::execute()
{
    if (pollFunc_)
        pollFunc_();
    else
        std::cerr << "[" << __func__ << "]: warning: no poll function defined for this item\n";
}

//===========================================================================//

EpicsPoller::EpicsPoller( double period )
                        : basePeriod_( period )
                        , tick_      ( 1      )
{
    threadId_ = epicsThreadCreate( "EpicsPoller"
                                 , epicsThreadPriorityMedium
                                 , epicsThreadGetStackSize(epicsThreadStackMedium)
                                 , threadFuncC
                                 , this
                                 );
}

//===========================================================================//

EpicsPoller::~EpicsPoller()
{
    running_.store(false);
}

//===========================================================================//

void EpicsPoller::setFast(bool fast)
{
    pollItemFast_.store( fast );
}

//===========================================================================//

void EpicsPoller::setRunning(bool running)
{
    this->running_.store(running);
}

//===========================================================================//

void EpicsPoller::addItem(std::unique_ptr<EpicsPollItem> item)
{
    pollItems_.push_back( std::move(item) );
}

//===========================================================================//

void EpicsPoller::threadFuncC(void *p)
{
    const int maxTick = 1000; // max tick 100s

    auto self = static_cast<EpicsPoller*>(p);

    while(!self->running_.load()){};

    while( self->running_.load() )
    {
        if ( self->pollItemFast_.load() )
        {
            for ( const auto& item : self->pollItems_ )
            {
                if (   (item->dividerFast_ > 0)
                    && (self->tick_ % item->dividerFast_ == 0) )
                {
                    item->execute();
                }
            }
        }
        else
        {
            for ( const auto& item : self->pollItems_ )
            {
                if (   (item->dividerSlow_ > 0)
                    && (self->tick_ % item->dividerSlow_ == 0)
                   )
                {
                    item->execute();
                }
            }
        }

        self->tick_++;
        if (self->tick_ >= maxTick) self->tick_ = 0;

        epicsThreadSleep( self->basePeriod_ );
    }

    std::cerr << "[" << __func__ << "]: : exiting\n";
}

//===========================================================================//
