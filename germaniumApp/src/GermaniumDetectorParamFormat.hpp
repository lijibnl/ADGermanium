/**
 * @file GermaniumDetectorParamFormat.hpp
 * @brief ZMQ message decode helpers for logging (shared with ZynqDetector).
 *
 * Provides human-readable formatting of ZMQ command/reply messages.
 * Decoder functions adapted from ZynqDetector's GermaniumParamFormat.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/12/2026
 * 
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#pragma once

//===========================================================================//

#include <string>
#include <cstdint>
#include "GermaniumDetectorTypes.hpp"
#include "GermaniumDetectorRegister.hpp"

//===========================================================================//

// Decodes command code to string
const char* decode_cmd(uint32_t cmd);

// Decodes register address to string
const char* decode_reg(uint32_t addr);

// Decodes global field id to string
const char* decode_global_field(uint16_t field_id);

// Decodes channel field id to string
const char* decode_channel_field(uint16_t field_id);

// Formats a ZmqCommandMsg as a human-readable string
std::string format_zmq_msg(const ZmqCommandMsg& msg);

//===========================================================================//
