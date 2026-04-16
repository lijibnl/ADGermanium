#pragma once

#include "germaniumDetectorRegister.hpp"
#include "germaniumDetectorTypes.hpp"

#include "EpicsPoller.hpp"

class germaniumDetector;

class germaniumDetectorPollItem : public PollItem
{
public:
    germaniumDetector& dev;
    uint32_t           opCode;
    uint32_t           addr;

    germaniumDetectorPollItem( germaniumDetector& dev
                             , uint32_t           opCode
                             , uint32_t           addr
                             , int                slowDivider
                             , int                fastDivider
                             );

    virtual void poll() override;

private:

    typedef struct
    {
        uint32_t opCode;
        uint32_t addr;
        int      slowDivider;
        int      fastDivider;
    }  PollInfo;

    static constexpr PollInfo pollInfo[] = { { ZMQ_CMD_REG_READ,      MARS_CALPULSE,   10, 1 }
                                           , { ZMQ_CMD_REG_READ,      CALPULSE_RATE,   10, 1 }
                                           , { ZMQ_CMD_REG_READ,      CALPULSE_CNT,    10, 1 }
                                           , { ZMQ_CMD_REG_READ,      CALPULSE_MODE,   10, 1 }
                                           , { ZMQ_CMD_REG_READ,      MARS_PIPE_DELAY, 10, 1 }
                                           , { ZMQ_CMD_REG_READ,      MARS_RDOUT_ENB,  10, 1 }
                                           , { ZMQ_CMD_REG_READ,      SIM_EVT_SEL,     10, 1 }
                                           , { ZMQ_CMD_REG_READ,      MARS_RDOUT_ENB,  10, 1 }
                                           , { ZMQ_CMD_REG_READ,      COUNT_MODE,      10, 1 }
                                           , { ZMQ_CMD_REG_READ,      TRIG,            10, 1 }
                                           , { ZMQ_CMD_REG_READ,      EVENT_TIME_CNTR, 10, 1 }
                                           , { ZMQ_CMD_REG_READ,      COUNT_TIME_LO,   10, 1 }
                                           , { ZMQ_CMD_REG_READ,      COUNT_TIME_HI,   10, 1 }

                                           , { ZMQ_CMD_I2C_TEMP_READ, 0,               10, 1 }
                                           , { ZMQ_CMD_I2C_TEMP_READ, 1,               10, 1 }
                                           , { ZMQ_CMD_I2C_TEMP_READ, 2,               10, 1 }

                                           , { ZMQ_CMD_I2C_ADC_READ,  ADC_CH_HV_RBV,   10, 1 }
                                           , { ZMQ_CMD_I2C_ADC_READ,  ADC_CH_HV_CUR,   10, 1 }
                                           , { ZMQ_CMD_I2C_ADC_READ,  ADC_CH_P1_CUR,   10, 1 }
                                           , { ZMQ_CMD_I2C_ADC_READ,  ADC_CH_P2_CUR,   10, 1 }
                                           };


};

class germaniumDetectorPoller : public EpicsPoller {
public:
    germaniumDetectorPoller(germaniumDetector& dev, double basePeriod = 1.0);

private:
    germaniumDetector& dev;
};
