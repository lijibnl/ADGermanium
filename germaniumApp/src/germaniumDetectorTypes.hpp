/**
 * @file germaniumDetectorTypes.hpp
 * @brief Type definitions, structures, and constants.
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
#include "germaniumDetectorRegister.hpp"

//===========================================================================//

// Driver-specific constants (not hardware registers)
#define GERMANIUM_CONTROL_PORT 8000
#define GERMANIUM_DATA_PORT 8001
#define SPECTRUM_SIZE 4096
#define TDC_SIZE 1024
#define UDP_BUFFER_SIZE 65536
#define UDP_MAX_STRING_SIZE 256
#define UDP_MAX_ARRAY_SIZE 8192
#define DATA_WRITE_BUFFER_SIZE (4*1024*1024)  // 4MB circular buffer for file writing

// Data packet types for UDP communication
#define PACKET_TYPE_SPECTRUM 0x01
#define PACKET_TYPE_EVENT 0x02
#define PACKET_TYPE_STATUS 0x03
#define PACKET_TYPE_RAW_DATA 0x04

// UDP Message IDs for different operations
#define UDP_MSG_ID_REGISTER_ACCESS 0x0001
#define UDP_MSG_ID_MARS_CONFIG 0x0002
#define UDP_MSG_ID_ADC_CONFIG 0x0003
#define UDP_MSG_ID_ARM_CONTROL 0x0004

//===========================================================================//

// MARS ASIC configuration structures - match original Mars_DDM
struct globalstr_t
{
    int st;      // Shaping time
    int g;       // Gain  
    int pol;     // Polarity
    int eblk;    // Bias current enable
    int gmon;    // Global monitor mode
    int puen;    // Pileup rejection enable
    int mfs;     // Multi-fire suppression
    int tds;     // TDC slope
    int tdm;     // TDC mode
    int th;      // Threshold
    int c;       // Monitor channel
    int m0;      // Monitor mode
    int saux;    // Auxiliary select
};

//===========================================================================//

struct channelstr_t
{
    int chen;    // Channel enable
    int tsen;    // Test pulse input enable
    int thtr;    // Threshold trim
    int putr;    // Pileup threshold trim
};

//===========================================================================//

// UDP protocol structures - Based on actual hardware specification
namespace DerivedNetwork {

    // Request payload structures
    struct SingleWordReqMsgPayload
    {
        uint32_t data;
    };

    struct __attribute__((__packed__)) AdcClkSkewReqMsgPayload
    {
        uint16_t chip_num;
        uint16_t skew;
    };

    struct StuffMarsReqMsgPayload
    {
        uint32_t loads[12][14];  // MARS configuration data
    };

    struct __attribute__((__packed__)) ZddmArmReqMsgPayload
    {
        uint16_t mode;
        uint16_t val;
    };

    union UdpReqMsgPayload
    {
        SingleWordReqMsgPayload  single_word;
        AdcClkSkewReqMsgPayload  ad9252_clk_skew;
        StuffMarsReqMsgPayload   stuff_mars;
        ZddmArmReqMsgPayload     zddm_arm;
    };

    // Response payload structures
    struct SingleWordRespMsgPayload
    {
        uint32_t data;
    };

    struct __attribute__((__packed__)) PsI2cRespMsgPayload
    {
        uint8_t length;
        uint8_t data[4];
    };

    struct __attribute__((__packed__)) PsXadcRespMsgPayload
    {
        uint8_t length;
        uint8_t data[4];
    };

    union UdpRespMsgPayload
    {
        SingleWordRespMsgPayload single_word;
        PsI2cRespMsgPayload      psi2c;
        PsXadcRespMsgPayload     psxadc;
    };

} // namespace DerivedNetwork

//===========================================================================//

// Main UDP message structures
struct __attribute__((__packed__)) UdpReqMsg
{   
    uint16_t                           id; 
    uint16_t                           op;
    DerivedNetwork::UdpReqMsgPayload   payload;
};  
using UdpRxMsg = UdpReqMsg;

//===========================================================================//

struct __attribute__((__packed__)) UdpRespMsg
{
    uint16_t                           id;
    uint16_t                           op;
    DerivedNetwork::UdpRespMsgPayload  payload;
};
using UdpTxMsg = UdpRespMsg;

//===========================================================================//

#define UDP_REQ_MSG_ID  = 0xbeef;
#define UDP_RESP_MSG_ID = 0xcafe;


// Helper functions for operation codes
#define UDP_OP_READ_BIT 0x8000
#define UDP_OP_WRITE_BIT 0x0000
#define UDP_OP_ADDR_MASK 0x7FFF

//===========================================================================//

inline uint16_t makeReadOp(uint16_t address) { return UDP_OP_READ_BIT | (address & UDP_OP_ADDR_MASK); }
inline uint16_t makeWriteOp(uint16_t address) { return UDP_OP_WRITE_BIT | (address & UDP_OP_ADDR_MASK); }
inline bool isReadOp(uint16_t op) { return (op & UDP_OP_READ_BIT) != 0; }
inline uint16_t getOpAddress(uint16_t op) { return op & UDP_OP_ADDR_MASK; }

//===========================================================================//

// Photon event data structure for UDP data packets
struct __attribute__((__packed__)) PhotonEvent
{
    uint16_t element;       // Detector element number
    uint16_t energy;        // Energy value
    uint32_t timestamp;     // Event timestamp
};

// Data packet header for received data
struct DataPacketHeader
{
    uint32_t packetType;    // Packet type (spectrum, event, status, etc.)
    uint32_t sequenceNumber; // Sequence number for packet ordering
    uint32_t timestamp;     // Hardware timestamp
    uint32_t dataLength;    // Length of data following header
};

// Event data structure
struct __attribute__((__packed__)) EventData
{
    uint16_t channel;       // Channel number
    uint16_t energy;        // Energy value
    uint32_t timestamp;     // Event timestamp
};

// Status data structure
struct StatusData
{
    uint32_t totalEvents;   // Total event count
    uint32_t totalTime;     // Total acquisition time
    uint32_t liveTime;      // Live time
    uint32_t deadTime;      // Dead time
    uint16_t temperature;   // System temperature
    uint16_t voltage;       // System voltage
};

// File writing structures
struct __attribute__((__packed__)) FileWriteBuffer
{
    uint8_t data[DATA_WRITE_BUFFER_SIZE];
    size_t writeIndex;
    size_t readIndex;
    size_t bytesUsed;
    bool overflow;
};

//===========================================================================//

