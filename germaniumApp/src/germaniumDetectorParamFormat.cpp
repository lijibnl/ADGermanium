/**
 * @file germaniumDetectorParamFormat.cpp
 * @brief ZMQ message decode helpers for logging.
 *
 * Adapted from ZynqDetector's GermaniumParamFormat.cpp to use
 * ADGermanium's ZmqCommandMsg and register/type definitions.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/12/2026
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "germaniumDetectorParamFormat.hpp"
#include <sstream>
#include <iomanip>

//===========================================================================//

const char* decode_cmd(uint32_t cmd)
{
    switch (cmd) {
        case ZMQ_CMD_REG_READ:          return "REG_READ";
        case ZMQ_CMD_REG_WRITE:         return "REG_WRITE";
        case ZMQ_CMD_MARS_SET_GLOBAL:   return "SET_GLOBAL";
        case ZMQ_CMD_MARS_SET_CHANNEL:  return "SET_CHANNEL";
        case ZMQ_CMD_MARS_LOAD:         return "MARS_LOAD";
        case ZMQ_CMD_ADC_CLK_SKEW:      return "ADC_CLK_SKEW";
        case ZMQ_CMD_I2C_TEMP_READ:     return "I2C_TEMP_READ";
        case ZMQ_CMD_XADC_READ:         return "XADC_READ";
        case ZMQ_CMD_I2C_DAC_WRITE:     return "I2C_DAC_WRITE";
        case ZMQ_CMD_I2C_ADC_READ:      return "I2C_ADC_READ";
        case ZMQ_CMD_I2C_DAC_INIT:      return "I2C_DAC_INIT";
        case ZMQ_CMD_SET_LOG_LEVEL:     return "SET_LOG_LEVEL";
        default: return "UNKNOWN";
    }
}

//===========================================================================//

const char* decode_reg(uint32_t addr)
{
    switch (addr) {
        case MARS_CONF_LOAD:    return "MARS_CONF_LOAD";
        case LEDS:              return "LEDS";
        case MARS_CONFIG:       return "MARS_CONFIG";
        case VERSIONREG:        return "VERSIONREG";
        case MARS_CALPULSE:     return "MARS_CALPULSE";
        case MARS_PIPE_DELAY:   return "MARS_PIPE_DELAY";
        case DETECTOR_MODEL:    return "DETECTOR_MODEL";
        case MARS_RDOUT_ENB:    return "MARS_RDOUT_ENB";
        case EVENT_TIME_CNTR:   return "EVENT_TIME_CNTR";
        case SIM_EVT_SEL:       return "SIM_EVT_SEL";
        case SIM_EVENT_RATE:    return "SIM_EVENT_RATE";
        case ADC_SPI:           return "ADC_SPI";
        case CALPULSE_CNT:      return "CALPULSE_CNT";
        case CALPULSE_RATE:     return "CALPULSE_RATE";
        case CALPULSE_WIDTH:    return "CALPULSE_WIDTH";
        case CALPULSE_MODE:     return "CALPULSE_MODE";
        case TD_CAL:            return "TD_CAL";
        case EVENT_FIFO_DATA:   return "EVENT_FIFO_DATA";
        case EVENT_FIFO_CNT:    return "EVENT_FIFO_CNT";
        case EVENT_FIFO_CTRL:   return "EVENT_FIFO_CTRL";
        case UDP_IP_ADDR:       return "UDP_IP_ADDR";
        case TRIG:              return "TRIG";
        case COUNT_TIME_LO:     return "COUNT_TIME_LO";
        case COUNT_TIME_HI:     return "COUNT_TIME_HI";
        case FRAME_NO:          return "FRAME_NO";
        case COUNT_MODE:        return "COUNT_MODE";
        default: return nullptr;
    }
}

//===========================================================================//

const char* decode_global_field(uint16_t field_id)
{
    switch (field_id) {
        case MARS_FIELD_ST:   return "ST";
        case MARS_FIELD_GAIN: return "GAIN";
        case MARS_FIELD_POL:  return "POL";
        case MARS_FIELD_EBLK: return "EBLK";
        case MARS_FIELD_GMON: return "GMON";
        case MARS_FIELD_PUEN: return "PUEN";
        case MARS_FIELD_MFS:  return "MFS";
        case MARS_FIELD_TDS:  return "TDS";
        case MARS_FIELD_TDM:  return "TDM";
        case MARS_FIELD_TH:   return "TH";
        case MARS_FIELD_C:    return "C";
        case MARS_FIELD_M0:   return "M0";
        case MARS_FIELD_SAUX: return "SAUX";
        default: return "?";
    }
}

//===========================================================================//

const char* decode_channel_field(uint16_t field_id)
{
    switch (field_id) {
        case MARS_CH_CHEN: return "CHEN";
        case MARS_CH_TSEN: return "TSEN";
        case MARS_CH_THTR: return "THTR";
        case MARS_CH_PUTR: return "PUTR";
        default: return "?";
    }
}

//===========================================================================//

std::string format_zmq_msg(const ZmqCommandMsg& msg)
{
    std::ostringstream oss;
    oss << decode_cmd(msg.cmd) << ": ";

    switch (msg.cmd) {
        case ZMQ_CMD_REG_READ:
        case ZMQ_CMD_REG_WRITE: {
            const char* regname = decode_reg(msg.addr);
            oss << (regname ? regname : "reg?")
                << " [0x" << std::hex << msg.addr << "]"
                << " value=0x" << std::hex << msg.value;
            break;
        }
        case ZMQ_CMD_MARS_SET_GLOBAL: {
            uint16_t chip_mask = (msg.addr >> 16) & 0x0FFF;
            uint16_t field_id  = msg.addr & 0xFFFF;
            oss << "chip_mask=0x" << std::hex << chip_mask
                << " field=" << decode_global_field(field_id)
                << " [" << field_id << "]"
                << " value=0x" << std::hex << msg.value;
            break;
        }
        case ZMQ_CMD_MARS_SET_CHANNEL: {
            uint16_t channel  = (msg.addr >> 16) & 0x0FFF;
            uint16_t field_id = msg.addr & 0xFFFF;
            oss << "channel=" << std::dec << channel
                << " field=" << decode_channel_field(field_id)
                << " [" << field_id << "]"
                << " value=0x" << std::hex << msg.value;
            break;
        }
        case ZMQ_CMD_MARS_LOAD: {
            uint16_t chip_mask = msg.addr & 0x0FFF;
            oss << "chip_mask=0x" << std::hex << chip_mask;
            break;
        }
        case ZMQ_CMD_SET_LOG_LEVEL: {
            oss << "level=" << msg.value;
            break;
        }
        default:
            oss << "addr=0x" << std::hex << msg.addr
                << " value=0x" << std::hex << msg.value;
    }
    return oss.str();
}

//===========================================================================//
