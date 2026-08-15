/**
 * @file GermaniumDetectorTypes.hpp
 * @brief Type definitions, structures, and constants for ADGermaniumZMQ.
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

#include <cstdint>
#include <cstddef>
#include <atomic>
// #include "GermaniumDetectorRegister.hpp"

#include "GermaniumDetectorProtocol.hpp"

//===========================================================================//

// ZMQ communication ports (matching ZynqDetector async-zmq)
#define ZMQ_CMD_PORT        "5555"    // PUSH-PULL for commands (IOC -> Zynq)
#define ZMQ_REPLY_PORT      "5557"    // PUSH-PULL for replies  (Zynq -> IOC)

// PL UDP data port (raw detector data from FPGA)
#define PL_UDP_DATA_PORT    0x7D03  // 32003

// Spectrum sizes
#define SPECTRUM_SIZE 4096
#define TDC_SIZE 1024

// Buffer sizes
#define UDP_BUFFER_SIZE 65536

//===========================================================================//

// Lock-free MPSC block queue for the data-write path.
// Two producers (ZMQ data thread, PL UDP thread) enqueue; one consumer
// (file-write thread) dequeues.  Producers claim slots via atomic
// fetch-add / CAS; per-block state flags ensure correct ordering.

static constexpr size_t DATA_BLOCK_SIZE     = 65536;                    // max payload per block
static constexpr int    DATA_QUEUE_BITS     = 7;                        // log2(capacity)
static constexpr int    DATA_QUEUE_CAPACITY = 1 << DATA_QUEUE_BITS;     // 128 blocks ≈ 8 MB
static constexpr int    DATA_QUEUE_MASK     = DATA_QUEUE_CAPACITY - 1;

static constexpr uint32_t DATA_BLOCK_FREE    = 0;
static constexpr uint32_t DATA_BLOCK_CLAIMED = 1;
static constexpr uint32_t DATA_BLOCK_READY   = 2;

struct DataBlock
{
    std::atomic<uint32_t> state;   // FREE → CLAIMED → READY → FREE
    uint32_t              size;    // actual payload bytes
    uint8_t               data[DATA_BLOCK_SIZE];
};


//=====================================================================//
// ZMQ command codes.
//=====================================================================//

#define ZMQ_CMD_REG_READ              GermaniumProtocol::Command::REG_READ
#define ZMQ_CMD_REG_WRITE             GermaniumProtocol::Command::REG_WRITE
#define ZMQ_CMD_MARS_GLOBAL_SET       GermaniumProtocol::Command::MARS_GLOBAL_SET
#define ZMQ_CMD_MARS_GLOBAL_READ      GermaniumProtocol::Command::MARS_GLOBAL_READ
#define ZMQ_CMD_MARS_CHANNEL_SET      GermaniumProtocol::Command::MARS_CHANNEL_SET
#define ZMQ_CMD_MARS_CHANNEL_READ     GermaniumProtocol::Command::MARS_CHANNEL_READ
#define ZMQ_CMD_MARS_LOAD             GermaniumProtocol::Command::MARS_LOAD
#define ZMQ_CMD_ADC_CLK_SKEW_SET      GermaniumProtocol::Command::ADC_CLK_SKEW_SET
#define ZMQ_CMD_I2C_TEMP_READ         GermaniumProtocol::Command::I2C_TEMP_READ
#define ZMQ_CMD_XADC_READ             GermaniumProtocol::Command::XADC_READ
#define ZMQ_CMD_I2C_DAC_WRITE         GermaniumProtocol::Command::I2C_DAC_WRITE
#define ZMQ_CMD_I2C_ADC_READ          GermaniumProtocol::Command::I2C_ADC_READ
#define ZMQ_CMD_I2C_DAC_INIT          GermaniumProtocol::Command::I2C_DAC_INIT
#define ZMQ_CMD_ADC_CLK_SKEW_READ     GermaniumProtocol::Command::ADC_CLK_SKEW_READ
#define ZMQ_CMD_SET_LOG_LEVEL         GermaniumProtocol::Command::SET_LOG_LEVEL
#define ZMQ_CMD_GET_PROTOCOL_VERSION  GermaniumProtocol::Command::GET_PROTOCOL_VERSION
#define ZMQ_CMD_HEARTBEAT             GermaniumProtocol::Command::HEARTBEAT

using ZmqCommandMsg = GermaniumProtocol::Message;
using GermaniumProtocol::MarsGlobalField;
using GermaniumProtocol::MarsChannelField;
using GermaniumProtocol::MARS_FIELD_ST;
using GermaniumProtocol::MARS_FIELD_GAIN;
using GermaniumProtocol::MARS_FIELD_POL;
using GermaniumProtocol::MARS_FIELD_EBLK;
using GermaniumProtocol::MARS_FIELD_GMON;
using GermaniumProtocol::MARS_FIELD_PUEN;
using GermaniumProtocol::MARS_FIELD_MFS;
using GermaniumProtocol::MARS_FIELD_TDS;
using GermaniumProtocol::MARS_FIELD_TDM;
using GermaniumProtocol::MARS_FIELD_TH;
using GermaniumProtocol::MARS_FIELD_TPAMP;
using GermaniumProtocol::MARS_FIELD_C;
using GermaniumProtocol::MARS_FIELD_M0;
using GermaniumProtocol::MARS_FIELD_SAUX;
using GermaniumProtocol::MARS_CH_CHEN;
using GermaniumProtocol::MARS_CH_TSEN;
using GermaniumProtocol::MARS_CH_THTR;
using GermaniumProtocol::MARS_CH_PUTR;
using GermaniumProtocol::MARS_CH_SEL;

//===========================================================================//

// PL UDP data markers
#define SOF_MARKER  0xFEEDFACE
#define EOF_MARKER  0xDECAFBAD

//===========================================================================//
