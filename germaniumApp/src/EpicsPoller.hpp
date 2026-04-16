#pragma once

#include <vector>
#include <memory>
#include <epicsThread.h>

//===========================================================================//

class PollItem
{
public:
    PollItem() = default;
    virtual ~PollItem() {}

    int divider;
    int dividerSlow;
    int dividerFast;

    virtual void poll() = 0;

    void setFast( bool fast );
};

//=============================================================================//

// ─────────────────────────────────────────────
// Poller class
// ─────────────────────────────────────────────
class EpicsPoller {
public:
    EpicsPoller( double basePeriod = 0.1 );
    ~EpicsPoller();

    void addItem(std::unique_ptr<PollItem> item);

private:
    static void threadFuncC(void *p);
    void threadFunc();

    std::vector<std::unique_ptr<PollItem>> pollItems;

    double basePeriod;
    int    tick;

    epicsThreadId threadId;
    std::atomic<bool> running;
};
