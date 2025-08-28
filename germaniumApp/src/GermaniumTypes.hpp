/*
 * GermaniumTypes.hpp
 * Type definitions, structures, and constants for Germanium detector driver
 * Shared across all Germanium source files
 */

#ifndef GERMANIUM_TYPES_HPP
#define GERMANIUM_TYPES_HPP

#include <cstdint>

// Hardware constants
#define GERMANIUM_CONTROL_PORT 8000
#define GERMANIUM_DATA_PORT 8001
#define SPECTRUM_SIZE 4096
#define TDC_SIZE 1024
#define UDP_BUFFER_SIZE 65536
#define UDP_MAX_STRING_SIZE 256
#define UDP_MAX_ARRAY_SIZE 8192
#define DATA_WRITE_BUFFER_SIZE (4*1024*1024)  // 4MB circular buffer for file writing

// Data packet types
#define PACKET_TYPE_SPECTRUM 0x01
#define PACKET_TYPE_EVENT 0x02
#define PACKET_TYPE_STATUS 0x03
#define PACKET_TYPE_RAW_DATA 0x04

// UDP Command codes
#define UDP_CMD_REGISTER_WRITE 0x01
#define UDP_CMD_REGISTER_READ 0x02
#define UDP_CMD_REGISTER_READ_RESPONSE 0x03
#define UDP_CMD_SET_FILENAME 0x10
#define UDP_CMD_LOAD_CALIBRATION 0x11
#define UDP_CMD_CHANNEL_ENABLE 0x20
#define UDP_CMD_THRESHOLD_ARRAY 0x21
#define UDP_CMD_CALIBRATION_ARRAY 0x22
#define UDP_CMD_MARS_GLOBAL 0x30
#define UDP_CMD_MARS_CHANNELS 0x31

// Register addresses (from pl.h analysis)
#define SIM_EVT_SEL 0x1000
#define CLR_EVT 0x1004
#define CLR_MON 0x1008
#define CLR_TIM 0x100C
#define STRT 0x1010
#define STOP 0x1014
#define COUNT_MODE 0x1018
#define COUNT_TIME_LO 0x101C
#define COUNT_TIME_HI 0x1020
#define MARS_CALPULSE 0x2000
#define CALPULSE_RATE 0x2004
#define CALPULSE_CNT 0x2008
#define CALPULSE_MODE 0x200C
#define MARS_PIPE_DELAY 0x2010
#define MARS_RDOUT_ENB 0x2014
#define TD_CAL 0x2018

// MARS ASIC global configuration structure
// This matches the original globalstr structure from Mars_DDM
struct MarsGlobalConfig
{
    uint8_t st;     // Shaping time
    uint8_t g;      // Gain
    uint8_t pol;    // Polarity
    uint8_t eblk;   // Enable input bias current
    uint16_t th;    // Threshold
    uint8_t puen;   // Pileup rejection enable
    uint8_t mfs;    // Multi-fire suppression
    uint8_t tds;    // TDS slope
    uint8_t tdm;    // TDC mode
    uint8_t gmon;   // Global monitor mode
    uint8_t c;      // Monitor channel
    uint8_t m0;     // Monitor mode bit
    uint8_t saux;   // Auxiliary select
    uint8_t reserved[3]; // Padding for alignment
};

// MARS ASIC channel configuration structure
// This matches the original channelstr structure from Mars_DDM
struct MarsChannelConfig
{
    uint8_t chen;   // Channel enable
    uint8_t tsen;   // Test pulse input enable
    uint8_t thtr;   // Threshold trim
    uint8_t putr;   // Pileup threshold trim
};

// UDP packet structures for communication - Based on actual protocol specification
namespace DerivedNetwork {

    // Request payload structures
    struct SingleWordReqMsgPayload
    {
        uint32_t data;
    };

    struct AdcClkSkewReqMsgPayload
    {
        uint16_t chip_num;
        uint16_t skew;
    };

    struct StuffMarsReqMsgPayload
    {
        uint32_t loads[12][14];
    };

    struct ZddmArmReqMsgPayload
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

    struct PsI2cRespMsgPayload
    {
        uint8_t length;
        uint8_t data[4];
    };

    struct PsXadcRespMsgPayload
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

// Main UDP message structures
struct UdpReqMsg
{   
    uint16_t                           id; 
    uint16_t                           op;
    DerivedNetwork::UdpReqMsgPayload   payload;
};  
using UdpRxMsg = UdpReqMsg;

struct UdpRespMsg
{
    uint16_t                           id;
    uint16_t                           op;
    DerivedNetwork::UdpRespMsgPayload  payload;
};
using UdpTxMsg = UdpRespMsg;

// Helper macros for operation codes
#define UDP_OP_READ_BIT 0x8000
#define UDP_OP_WRITE_BIT 0x0000
#define UDP_OP_ADDR_MASK 0x7FFF

// Operation code constructors
inline uint16_t makeReadOp(uint16_t address) { return UDP_OP_READ_BIT | (address & UDP_OP_ADDR_MASK); }
inline uint16_t makeWriteOp(uint16_t address) { return UDP_OP_WRITE_BIT | (address & UDP_OP_ADDR_MASK); }
inline bool isReadOp(uint16_t op) { return (op & UDP_OP_READ_BIT) != 0; }
inline uint16_t getOpAddress(uint16_t op) { return op & UDP_OP_ADDR_MASK; }

// Legacy UDP packet structures (kept for compatibility)
struct UDPPacket
{
    uint32_t command;
    uint32_t address;
    uint32_t data;
    uint32_t length;
};

struct UDPCommand
{
    uint32_t command;
    uint32_t address;
    uint32_t data;
    uint32_t checksum;
    uint32_t data_length;  // For variable length payloads
};

struct UDPResponse
{
    uint32_t status;
    uint32_t address;
    uint32_t data;
    uint32_t timestamp;
};

// Photon event data structure for UDP data packets
struct PhotonEvent
{
    uint16_t element;       // Detector element number
    uint16_t energy;        // Energy value
    uint32_t timestamp;     // Event timestamp
};

struct UDPStringPacket
{
    uint32_t command;
    uint32_t length;
    char data[UDP_MAX_STRING_SIZE];
};

struct UDPArrayPacket
{
    uint32_t command;
    uint32_t param;
    uint32_t length;
    uint8_t data[UDP_MAX_ARRAY_SIZE];
};

struct UDPMarsGlobalPacket
{
    uint32_t command;
    uint32_t chip;
    MarsGlobalConfig config;
};

struct UDPMarsChannelPacket
{
    uint32_t command;
    uint32_t startChannel;
    uint32_t numChannels;
    MarsChannelConfig channels[UDP_MAX_ARRAY_SIZE / sizeof(MarsChannelConfig)];
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
struct EventData
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

#endif // GERMANIUM_TYPES_HPP
