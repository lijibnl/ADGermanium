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

static constexpr size_t DATA_BLOCK_SIZE     = 1024;                     // max payload per block
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


//===========================================================================//

// PL UDP data markers
#define SOF_MARKER  0xFEEDFACE
#define EOF_MARKER  0xDECAFBAD

//===========================================================================//
