#include "germaniumDetectorPoller.hpp"

//===========================================================================//

germaniumDetectorPollItem::germaniumDetectorPollItem( germaniumDetector& dev
                                                    , uint32_t           opCode
                                                    , uint32_t           addr
                                                    , int                slowDivider
                                                    , int                fastDivider
                                                    )
                                                    : dev        ( dev         )
                                                    , opCode     ( opCode      )
                                                    , addr       ( addr        )
                                                    , dividerSlow( slowDivider )
                                                    , dividerFast( fastDivider )
                                                    , divider    ( slowDivider )
{}

//===========================================================================//

void germaniumDetectorPollItem::poll()
{
    dev.zmqTx( opCode, addr, 0);
}

//===========================================================================//

germaniumDetectorPoller::germaniumDetectorPoller( germaniumDetector& dev
                                                , double             basePeriod )
                                                : EpicsPoller( basePeriod )
{
    for ( auto& item : pollInfo )
        addItem( std::make_unique<germaniumDetectorPollItem>( dev
                                                            , item.opCode
                                                            , item.addr
                                                            , item.slowDivider
                                                            , item.fastDivider
                                                            ));

}
