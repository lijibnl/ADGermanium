#include <atomic>
#include <stdio.h>
//#include <print>
#include <iostream>

#include "EpicsPoller.hpp"

//===========================================================================//

EpicsPollItem::EpicsPollItem( int slowDivider, int fastDivider, PollFunc pollFunc )
                            : dividerSlow( slowDivider )
                            , dividerFast( fastDivider )
                            , divider    ( slowDivider )
                            , pollFunc   ( pollFunc    )
{}

//===========================================================================//

void EpicsPollItem::execute()
{
    if (pollFunc)
        pollFunc();
    else
        std::cerr << "[" << __func__ << "]: warning: no poll function defined for this item\n";
}

//===========================================================================//

EpicsPoller::EpicsPoller( double period )
                        : basePeriod( period )
                        , tick      ( 1      )
{
    threadId = epicsThreadCreate( "EpicsPoller"
                                 , epicsThreadPriorityMedium
                                 , epicsThreadGetStackSize(epicsThreadStackMedium)
                                 , threadFuncC
                                 , this
                                 );
}

//===========================================================================//

EpicsPoller::~EpicsPoller()
{
    running.store(false);
}

//===========================================================================//

void EpicsPoller::setFast(bool fast)
{
    pollItemFast.store( fast );
}

//===========================================================================//

void EpicsPoller::setFunning(bool running)
{
    this->running.store(running);
}

//===========================================================================//

void EpicsPoller::addItem(std::unique_ptr<EpicsPollItem> item)
{
    pollItems.push_back( std::move(item) );
}

//===========================================================================//

void EpicsPoller::threadFuncC(void *p)
{
    const int maxTick = 1000; // max tick 100s

    auto self = static_cast<EpicsPoller*>(p);

    while( self->running.load() )
    {
        if ( self->pollItemFast.load() )
        {
            for ( auto& item : self->pollItems )
            {
                if (   (item->dividerFast > 0)
                    && (self->tick % item->dividerFast == 0) )
                {
                    item->execute();
                }
            }
        }
        else
        {
            for ( auto& item : self->pollItems )
            {
                if (   (item->dividerSlow > 0)
                    && (self->tick % item->dividerSlow == 0)
                   )
                {
                    item->execute();
                }
            }
        }

        self->tick++;
        if (self->tick >= maxTick) self->tick = 0;

        epicsThreadSleep( self->basePeriod );
    }

    std::cerr << "[" << __func__ << "]: : exiting\n";
}

//===========================================================================//
