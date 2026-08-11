/**
 * @file GermaniumDetectorRegister.hpp
 * @brief FPGA register definitions. Originated from Germanium detector
 * protocol.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * 
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#pragma once

//===========================================================================//

#include "GermaniumDetectorProtocol.hpp"

//===========================================================================//

// FPGA register addresses (used as 'addr' field in ZMQ commands)
#define MARS_CONF_LOAD      GermaniumProtocol::Register::MARS_CONF_LOAD
#define LEDS                GermaniumProtocol::Register::LEDS
#define MARS_CONFIG         GermaniumProtocol::Register::MARS_CONFIG
#define VERSIONREG          GermaniumProtocol::Register::VERSIONREG
#define MARS_CALPULSE       GermaniumProtocol::Register::MARS_CALPULSE
#define MARS_PIPE_DELAY     GermaniumProtocol::Register::MARS_PIPE_DELAY
#define DETECTOR_MODEL      GermaniumProtocol::Register::DETECTOR_MODEL
#define MARS_RDOUT_ENB      GermaniumProtocol::Register::MARS_RDOUT_ENB
#define EVENT_TIME_CNTR     GermaniumProtocol::Register::EVENT_TIME_CNTR
#define SIM_EVT_SEL         GermaniumProtocol::Register::SIM_EVT_SEL
#define SIM_EVENT_RATE      GermaniumProtocol::Register::SIM_EVENT_RATE
#define ADC_SPI             GermaniumProtocol::Register::ADC_SPI
#define CALPULSE_CNT        GermaniumProtocol::Register::CALPULSE_CNT
#define CALPULSE_RATE       GermaniumProtocol::Register::CALPULSE_RATE
#define CALPULSE_WIDTH      GermaniumProtocol::Register::CALPULSE_WIDTH
#define CALPULSE_MODE       GermaniumProtocol::Register::CALPULSE_MODE
#define TD_CAL              GermaniumProtocol::Register::TD_CAL
#define EVENT_FIFO_DATA     GermaniumProtocol::Register::EVENT_FIFO_DATA
#define EVENT_FIFO_CNT      GermaniumProtocol::Register::EVENT_FIFO_CNT
#define EVENT_FIFO_CTRL     GermaniumProtocol::Register::EVENT_FIFO_CTRL
#define UDP_IP_ADDR         GermaniumProtocol::Register::UDP_IP_ADDR
#define TRIG                GermaniumProtocol::Register::TRIG
#define COUNT_TIME_LO       GermaniumProtocol::Register::COUNT_TIME_LO
#define COUNT_TIME_HI       GermaniumProtocol::Register::COUNT_TIME_HI
#define FRAME_NO            GermaniumProtocol::Register::FRAME_NO
#define COUNT_MODE          GermaniumProtocol::Register::COUNT_MODE

// I2C DAC7678 channel indices (used as addr in I2C_DAC_WRITE)
#define DAC_CH_HV           5       // High voltage
#define DAC_CH_P1           6       // Peltier 1
#define DAC_CH_P2           2       // Peltier 2

// I2C LTC2309 channel indices (used as addr in I2C_ADC_READ)
#define ADC_CH_HV_RBV       4       // HV voltage readback
#define ADC_CH_HV_CUR       5       // HV current
#define ADC_CH_P1_CUR       6       // Peltier 1 current
#define ADC_CH_P2_CUR       7       // Peltier 2 current
