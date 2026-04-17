/**
 * @file germaniumDetectorTypes.hpp
 * @brief Type definitions, structures, and constants for ADGermaniumZMQ.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
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
#include "germaniumDetectorRegister.hpp"

//===========================================================================//

// ZMQ communication ports (matching ZynqDetector async-zmq)
#define ZMQ_CMD_PORT        5555    // PUSH-PULL for commands (IOC→Zynq)
#define ZMQ_REPLY_PORT      5557    // PUSH-PULL for replies  (Zynq→IOC)
#define ZMQ_DATA_PORT       5556    // PUB-SUB for event data

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

// ZMQ command codes — register ops (matching germ-zmq-server)
// Register addr is a word offset (see germaniumDetectorRegister.hpp).
#define ZMQ_CMD_REG_READ    0x0
#define ZMQ_CMD_REG_WRITE   0x1

// ZMQ command codes — MARS delta-config protocol
// The IOC sends lightweight per-field deltas; the Zynq server
// (germ-zmq-server) maintains chipstr/chanstr state, packs into
// loads[12][14] via wrap(), and writes to the MARS ASICs.
//
// Message format (same 3×uint32 ZmqCommandMsg):
//
//   CMD_MARS_SET_GLOBAL:
//     cmd   = 0x10
//     addr  = chip_mask[27:16] | field_id[15:0]
//     value = new value
//     chip_mask: 12-bit, one bit per chip (0xFFF = all)
//     field_id:  MarsGlobalField enum
//
//   CMD_MARS_SET_CHANNEL:
//     cmd   = 0x11
//     addr  = channel[27:16] | field_id[15:0]
//     value = new value
//     channel: 0..383, or 0xFFF = all channels
//
//   CMD_MARS_LOAD:
//     cmd   = 0x12
//     addr  = chip_mask[11:0]
//     value = 0
//     Triggers wrap() + stuff_mars() on Zynq for selected chips.
//
#define ZMQ_CMD_MARS_SET_GLOBAL  0x10
#define ZMQ_CMD_MARS_SET_CHANNEL 0x11
#define ZMQ_CMD_MARS_LOAD        0x12
#define ZMQ_CMD_ADC_CLK_SKEW     0x20
#define ZMQ_CMD_I2C_TEMP_READ    0x21
#define ZMQ_CMD_XADC_READ        0x22
#define ZMQ_CMD_I2C_DAC_WRITE    0x23
#define ZMQ_CMD_I2C_ADC_READ     0x24
#define ZMQ_CMD_I2C_DAC_INIT     0x25
#define ZMQ_CMD_SET_LOG_LEVEL    0x30

// Field IDs for CMD_MARS_SET_GLOBAL
enum MarsGlobalField
{
    MARS_FIELD_ST   = 0,
    MARS_FIELD_GAIN = 1,
    MARS_FIELD_POL  = 2,
    MARS_FIELD_EBLK = 3,
    MARS_FIELD_GMON = 4,
    MARS_FIELD_PUEN = 5,
    MARS_FIELD_MFS  = 6,
    MARS_FIELD_TDS  = 7,
    MARS_FIELD_TDM  = 8,
    MARS_FIELD_TH   = 9,
    MARS_FIELD_C    = 10,
    MARS_FIELD_M0   = 11,
    MARS_FIELD_SAUX = 12,
};

// Field IDs for CMD_MARS_SET_CHANNEL
enum MarsChannelField
{
    MARS_CH_CHEN = 0,
    MARS_CH_TSEN = 1,
    MARS_CH_THTR = 2,
    MARS_CH_PUTR = 3,
};

// ZMQ message structure: 3 x uint32_t
struct ZmqCommandMsg
{
    uint32_t cmd;       // Command code (0x00..0x12)
    uint32_t addr;      // Register word offset or field encoding
    uint32_t value;     // Data value
};

//===========================================================================//

// PL UDP data markers
#define SOF_MARKER  0xFEEDFACE
#define EOF_MARKER  0xDECAFBAD

//===========================================================================//
