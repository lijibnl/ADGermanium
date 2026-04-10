/**
 * @file germaniumDetectorRegister.hpp
 * @brief FPGA register definitions. Originated from GeRM FPGA design code.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//
#pragma once

// FPGA register addresses (used as 'addr' field in ZMQ commands)
#define MARS_CONF_LOAD      0
#define LEDS                1
#define MARS_CONFIG         2
#define VERSIONREG          3
#define MARS_CALPULSE       4
#define MARS_PIPE_DELAY     5
#define DETECTOR_TYPE       6
#define MARS_RDOUT_ENB      8
#define EVENT_TIME_CNTR     9
#define SIM_EVT_SEL         10
#define SIM_EVENT_RATE      11
#define ADC_SPI             12
#define CALPULSE_CNT        16
#define CALPULSE_RATE       17
#define CALPULSE_WIDTH      18
#define CALPULSE_MODE       19
#define TD_CAL              20
#define EVENT_FIFO_DATA     24
#define EVENT_FIFO_CNT      25
#define EVENT_FIFO_CTRL     26
#define UDP_IP_ADDR         40
#define TRIG                52
#define COUNT_TIME_LO       53
#define COUNT_TIME_HI       54
#define FRAME_NO            55
#define COUNT_MODE          56

// I2C DAC7678 channel indices (used as addr in I2C_DAC_WRITE)
#define DAC_CH_HV           5       // High voltage
#define DAC_CH_P1           6       // Peltier 1
#define DAC_CH_P2           2       // Peltier 2

// I2C LTC2309 channel indices (used as addr in I2C_ADC_READ)
#define ADC_CH_HV_RBV       4       // HV voltage readback
#define ADC_CH_HV_CUR       5       // HV current
#define ADC_CH_P1_CUR       6       // Peltier 1 current
#define ADC_CH_P2_CUR       7       // Peltier 2 current
