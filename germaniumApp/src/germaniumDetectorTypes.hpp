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
//{
//    int st;      // Shaping time
//    int g;       // Gain  
//    int pol;     // Polarity
//    int eblk;    // Bias current enable
//    int gmon;    // Global monitor mode
//    int puen;    // Pileup rejection enable
//    int mfs;     // Multi-fire suppression
//    int tds;     // TDC slope
//    int tdm;     // TDC mode
//    int th;      // Threshold
//    int c;       // Monitor channel
//    int m0;      // Monitor mode
//    int saux;    // Auxiliary select
//};
//struct chipstr
{
	unsigned int pa      {1023};  /* Threshold dac */
	unsigned int pb      {0};	  /* Test pulse dac */
	unsigned char rm     {1};	  /* Readout mods; 1=synch, 0=asynch */
	unsigned char senfl1 {0};     /* Lock on peak found */
	unsigned char senfl2 {1};     /* Lock on threshold */
	unsigned char m0     {0};	  /* 1=channel mon, 0=others */
	unsigned char m1     {1};	  /* 1=pk det on PD/PN; 0=other mons on PD/PN */
	unsigned char sbn    {0};	  /* enable buffer on pdn & mon outputs */
	unsigned char sb     {1};	  /* enable buffer on pd & mon outputs */
	unsigned char sl     {0};	  /* 0=internal 2pA leakage, 1=disabled */
	unsigned char ts     {1};	  /* Shaping time */
	unsigned char rt     {0};	  /* 1=timing ramp duration x 3 */
	unsigned char spur   {1};	  /* 1=enable pileup rejector */
	unsigned char sse    {0};	  /* 1=enable multiple-firing suppression */
	unsigned char tr     {1};	  /* timing ramp adjust */
	unsigned char ss     {2};	  /* multiple firing time adjust */
	unsigned char c      {31};	  /* m0=0,Monitor select. m0=1, channel being monitored */
	unsigned char g      {1};	  /* Gain select */
	unsigned char slh    {0};	  /* internal leakage adjust */
	unsigned char sp     {1};	  /* Input polarity; 1=positive, 0=negative */
	unsigned char saux   {0};	  /* Enable monitor output */
	unsigned char sbm    {1};	  /* Enable output monitor buffer */
	unsigned char tm     {0};	  /* Timing mode; 0=ToA, 1=ToT */
};
//===========================================================================//

struct channelstr_t
//{
//    int chen;    // Channel enable
//    int tsen;    // Test pulse input enable
//    int thtr;    // Threshold trim
//    int putr;    // Pileup threshold trim
//};
//struct chanstr
{
	unsigned char dp  {15};  /* Pileup rejector trim dac */
	unsigned char nc1 {0};   /* no connection, set 0 */
	unsigned char da  {5};   /* Threshold trim dac */
	unsigned char sel {1};   /* 1=leakage current, 0=shaper output */
	unsigned char nc2 {0};   /* no connection, set 0 */
	unsigned char sm  {1};   /* 1=channel disable */
	unsigned char st  {0};   /* 1=enable test input (30fF cap) */
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

