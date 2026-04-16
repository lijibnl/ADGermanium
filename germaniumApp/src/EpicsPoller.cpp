#include "EpicsPoller.h"

//===========================================================================//

void PollItem::setFast(bool fast)
{
    divider = fast ? dividerFast : dividerSlow;
}

//===========================================================================//

EpicsPoller::EpicsPoller( double period )
                        : basePeriod( period )
                        , tick      ( 0      )
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

void EpicsPoller::addItem(const PollItem& item)
{
    pollItems.emplace_back( item );
}

//===========================================================================//

void EpicsPoller::threadFuncC(void *p)
{
    const int maxTick = 1000; // max tick 100s

    while(running.load())
    {
        for (const auto& item : pollItems)
        {
            if ( (tick % item->divider > 0) && (tick % item->dividerFast == 0) )
            {
                item->poll();
            }
        }

        tick++;
        if (tick >= maxTick) tick = 0;

        epicsThreadSleep(basePeriod);
    }
}

//===========================================================================//
