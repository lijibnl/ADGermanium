/**
 * @file GermaniumDetectorZmq.cpp
 * @brief Async ZMQ communication with ZynqDetector (PUSH-PULL).
 *
 * Command channel: IOC PUSH → Zynq PULL on port 5555
 * Reply channel:   Zynq PUSH → IOC PULL on port 5557
 *
 * Threads:
 *   Tx thread:      drains txQueue_, sends via tx socket
 *   Rx thread:      receives from PULL socket, updates PV cache
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * 
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <iostream>
#include <arpa/inet.h>

#include "GermaniumDetector.hpp"
#include "GermaniumDetectorParamFormat.hpp"
#include "GermaniumProtocolCompatibility.hpp"
#include "Zmq.hpp"

//===========================================================================//

bool GermaniumDetector::initializeZmq()
{
    std::cout << "[" << __func__ << "]: initializing ZMQ messaging to " << ipAddress << "\n";

    // Initialize ZMQ client
    try
    {
        zmqClient = std::make_unique<ZmqClient>( zmqContext
                                               , zmqTxEndpoint
                                               , zmqRxEndpoint
                                               );
    }
    catch (const zmq::error_t& e)
    {
        std::cerr << "ZMQ error: " << e.what() << "\n";
        return false;

    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return false;

    }
    catch (...)
    {
        std::cerr << "Unknown error\n";
        return false;
    }

    //--------------------------------------------------------------
    // Tx queue infrastructure
    //--------------------------------------------------------------
    txQueueMutex_ = epicsMutexCreate();
    txQueueEvent_ = epicsEventCreate(epicsEventEmpty);

    //--------------------------------------------------------------
    // Start Tx and Rx threads
    //--------------------------------------------------------------
    zmqTxThreadId = epicsThreadCreate( "zmqTx"
                                     , epicsThreadPriorityMedium
                                     , epicsThreadGetStackSize(epicsThreadStackMedium)
                                     , zmqTxThreadC
                                     , this
                                     );

    if ( !zmqTxThreadId )
    {
        std::cerr << "[" << __func__ << "]: failed to create Tx thread\n";
        return false;
    }
    std::cout << "[" << __func__ << "]: Tx thread started\n";

    zmqRxThreadId = epicsThreadCreate( "zmqRx"
                                     , epicsThreadPriorityMedium
                                     , epicsThreadGetStackSize(epicsThreadStackMedium)
                                     , zmqRxThreadC
                                     , this
                                     );

    if ( !zmqRxThreadId )
    {
        std::cerr << "[" << __func__ << "]: failed to create ZMQ Rx thread\n";
        return false;
    }
    std::cout << "[" << __func__ << "]: ZMQ Rx threads started\n";

    //--------------------------------------------------------------
    // Read startup PVs which will never need be read again.
    //--------------------------------------------------------------

    std::cout << "[" << __func__ << "]: ZMQ initialized\n";
    std::cout << "                 - Tx: " << zmqTxEndpoint << "\n";
    std::cout << "                 - Rx: " << zmqRxEndpoint << "\n";

    return true;
}

//===========================================================================//

asynStatus GermaniumDetector::zmqTx(uint32_t cmd, uint32_t addr, uint32_t value)
{
    TxQueueItem item;
    item.msg.cmd   = cmd;
    item.msg.addr  = addr;
    item.msg.value = value;

    epicsMutexLock(txQueueMutex_);
    txQueue_.push_back(item);
    epicsMutexUnlock(txQueueMutex_);

    epicsEventSignal(txQueueEvent_);
    return asynSuccess;
}

//===========================================================================//

void GermaniumDetector::requestProtocolVersion()
{
    zmqTx(ZMQ_CMD_GET_PROTOCOL_VERSION, 0, 0);
}

//===========================================================================//

void GermaniumDetector::zmqSend(const ZmqCommandMsg& msg)
{
    asynPrint( pasynUserSelf
             , ASYN_TRACEIO_DRIVER
             , "[%s]: ZMQ TX: cmd=0x%02X addr=0x%04X value=0x%08X\n"
             , portName
             , msg.cmd
             , msg.addr
             , msg.value
             );

    asynPrint( pasynUserSelf
             , ASYN_TRACEIO_DRIVER
             , "[%s]: ZMQ TX (decoded): %s\n"
             , portName
             , format_zmq_msg(msg).c_str()
             );

    zmqClient->tx<ZmqCommandMsg>( msg );
}

//===========================================================================//

asynStatus GermaniumDetector::zmqMarsSetGlobal( uint32_t chipMask
                                              , MarsGlobalField field
                                              , uint32_t value
                                              )
{
    return zmqTx( ZMQ_CMD_MARS_GLOBAL_SET
                , (chipMask << 16) | static_cast<uint32_t>(field)
                , value
                );
}

//===========================================================================//

asynStatus GermaniumDetector::zmqMarsSetChannel( uint32_t channel
                                               , MarsChannelField field
                                               , uint32_t value
                                               )
{
    return zmqTx( ZMQ_CMD_MARS_CHANNEL_SET
                , (channel << 16) | static_cast<uint32_t>(field)
                , value
                );
}

//===========================================================================//

asynStatus GermaniumDetector::zmqMarsLoad(uint32_t chipMask)
{
    return zmqTx( ZMQ_CMD_MARS_LOAD
                , chipMask
                , 0
                );
}

//===========================================================================//
//  Tx thread: drains tx queue, sends via tx socket
//===========================================================================//

void GermaniumDetector::zmqTxThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->zmqTxThread();
}

void GermaniumDetector::zmqTxThread()
{
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "[%s]: Tx thread started\n", portName);

    while ( threadsRunning.load() )
    {
        epicsEventWaitWithTimeout(txQueueEvent_, 0.1);

        if ( zmqNeedReset.exchange(false) )
        {
            asynPrint( pasynUserSelf
                     , ASYN_TRACE_ERROR
                     , "[%s]: Tx thread resetting ZMQ client\n"
                     , portName
                     );
            zmqClient->resetTxSocket();
            continue;
        }

        // Drain the queue
        std::vector<TxQueueItem> batch;
        epicsMutexLock(txQueueMutex_);
        batch.swap(txQueue_);
        epicsMutexUnlock(txQueueMutex_);

        if ( zmqServerDown.load() )
        {
            asynPrint( pasynUserSelf
                     , ASYN_TRACE_ERROR
                     , "[%s]: Tx thread detected server down, skipping send\n"
                     , portName
                     );

            // In server-down status, only send heartbeat messages
            auto it = std::ranges::find(batch, ZMQ_CMD_HEARTBEAT, [](const auto& item) {
                                                                     return item.msg.cmd;
                                                                    });

            if (it != batch.end())
            {
                zmqClient->tx<ZmqCommandMsg>(it->msg);
            }
        }
        else
        {
            for (auto& item : batch)
            {
                zmqClient->tx<ZmqCommandMsg>(item.msg);
            }
        }
    }

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "[%s]: Tx thread stopped\n"
             , portName
             );
}

//===========================================================================//
//  ZMQ Rx thread: receives replies, updates PV cache
//===========================================================================//

void GermaniumDetector::zmqRxThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->zmqRxThread();
}

void GermaniumDetector::zmqRxThread()
{
    std::cout << "[" << __func__ << "]: ZMQ Rx thread started\n";

    while ( threadsRunning.load() )
    {
        ZmqCommandMsg reply;
        auto rs = zmqClient->rx<ZmqCommandMsg>( reply );
        if ( rs == ZmqClient::RecvStatus::Timeout )
        {
            // Set server-down state on timeout
            zmqServerDown.store(true);
            continue;
        }
        else if ( rs == ZmqClient::RecvStatus::SizeMismatch )
        {
            // Wrong size
            continue;
        }

        if ( zmqServerDown.load() )
        {
            // Receive in server-down state, Tx should reset the socket.
            asynPrint( pasynUserSelf
                     , ASYN_TRACE_ERROR
                     , "[%s]: detector ZMQ server recovered\n", portName);
            zmqServerDown.store(false);
            zmqNeedReset.store(true);

            // Re-read parameters that are initialized by the detector,
            // since the Zynq may have rebooted with fresh defaults.
            readInitParams();
            requestUdpReinitialization();
        }

        asynPrint( pasynUserSelf
                 , ASYN_TRACEIO_DRIVER
                 , "[%s]: ZMQ RX: cmd=0x%02X addr=0x%04X value=0x%08X\n"
                 , portName
                 , reply.cmd
                 , reply.addr
                 , reply.value
                 );
        asynPrint( pasynUserSelf
                 , ASYN_TRACEIO_DRIVER
                 , "[%s]: ZMQ RX (decoded): %s\n"
                 , portName
                 , format_zmq_msg(reply).c_str()
                 );

        processReply(reply);
    }

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "[%s]: ZMQ Rx thread stopped\n"
             , portName
             );
}

//===========================================================================//

void GermaniumDetector::processReply(const ZmqCommandMsg& reply)
{

    switch (reply.cmd)
    {
        case ZMQ_CMD_GET_PROTOCOL_VERSION:
            processReplyProtocolVersion( reply.value );
            break;

        case ZMQ_CMD_REG_READ:
            processReplyRegRead( reply.addr, reply.value );
            break;

        case ZMQ_CMD_REG_WRITE:
            processReplyRegWrite( reply.addr, reply.value );
            break;

        case ZMQ_CMD_I2C_TEMP_READ:
            processReplyI2cTempRead( reply.addr, reply.value );
            break;


        case ZMQ_CMD_XADC_READ:
            processReplyXadcRead( reply.addr, reply.value );
            break;

        case ZMQ_CMD_I2C_ADC_READ:
            processReplyI2cAdcRead( reply.addr, reply.value );
            break;

        case ZMQ_CMD_MARS_GLOBAL_SET:
            processReplyMarsGlobalSet( reply.addr, reply.value );
            break;

        case ZMQ_CMD_MARS_GLOBAL_READ:
            processReplyMarsGlobalRead( reply.addr, reply.value );
            break;

        case ZMQ_CMD_MARS_CHANNEL_SET:
            processReplyMarsChannelSet( reply.addr, reply.value );
            break;

        case ZMQ_CMD_MARS_CHANNEL_READ:
            processReplyMarsChannelRead( reply.addr, reply.value );
            break;

        case ZMQ_CMD_ADC_CLK_SKEW_SET:
            processReplyAdcClkSkewSet( reply.addr, reply.value );
            break;

        case ZMQ_CMD_ADC_CLK_SKEW_READ:
            processReplyAdcClkSkewRead( reply.addr, reply.value );
            break;

        case ZMQ_CMD_I2C_DAC_WRITE:
            processReplyI2cDacWrite( reply.addr, reply.value );
            break;

        case ZMQ_CMD_I2C_DAC_INIT:
            processReplyI2cDacInit( reply.addr, reply.value );
            break;

        case ZMQ_CMD_HEARTBEAT:
            // No action needed for heartbeat replies
            break;

        default:
            break;
    }

    callParamCallbacks();
}

//===========================================================================//

void GermaniumDetector::processReplyProtocolVersion( uint32_t value )
{
    constexpr uint32_t expected = GermaniumProtocol::PROTOCOL_VERSION;

    const uint16_t reportedMajor = GermaniumProtocol::protocolVersionMajor(value);
    const uint16_t reportedMinor = GermaniumProtocol::protocolVersionMinor(value);
    const uint16_t expectedMajor = GermaniumProtocol::PROTOCOL_MAJOR;
    const uint16_t expectedMinor = GermaniumProtocol::PROTOCOL_MINOR;

    if ( value == expected )
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_FLOW
                 , "[%s]: detector protocol version %u.%u verified\n"
                 , portName
                 , expectedMajor
                 , expectedMinor
                 );
        return;
    }

    const char* recommendation =
        GermaniumProtocolCompatibility::recommendedAdGermaniumVersion(reportedMajor, reportedMinor);

    std::cerr << "\nERROR: Germanium protocol mismatch.\n\n"
              << "ZynqDetector reports protocol "
              << reportedMajor << "." << reportedMinor
              << " (0x" << std::hex << value << std::dec << ").\n"
              << "This ADGermanium build requires protocol "
              << expectedMajor << "." << expectedMinor
              << " (0x" << std::hex << expected << std::dec << ").\n\n"
              << "This IOC cannot safely control this detector.\n";

    if ( recommendation )
    {
        std::cerr << "Use " << recommendation << " for this detector,\n"
                  << "or update ZynqDetector to protocol "
                  << expectedMajor << "." << expectedMinor << ".\n";
    }
    else
    {
        std::cerr << "No ADGermanium release recommendation is recorded for protocol "
                  << reportedMajor << "." << reportedMinor << " in this build.\n"
                  << "Use an ADGermanium release built for that protocol,\n"
                  << "or update ZynqDetector to protocol "
                  << expectedMajor << "." << expectedMinor << ".\n";
    }

    std::cerr << "Exiting.\n";
    std::exit(EXIT_FAILURE);
}

//===========================================================================//

void GermaniumDetector::processReplyRegRead( uint32_t addr, uint32_t value )
{
    static uint32_t count_time_lo = 0;
    static uint32_t count_time_hi = 0;

    switch( addr )
    {
        case VERSIONREG:
            setIntegerParam(GermaniumFVER, static_cast<int>(value));
            break;
        case DETECTOR_MODEL:
            setIntegerParam(GermaniumDETMODEL, static_cast<int>(value));
            break;
        case MARS_CALPULSE:
            setIntegerParam(GermaniumTPAMP_RBV, static_cast<int>(value));
            break;
        case CALPULSE_RATE:
            setIntegerParam(GermaniumTPFRQ_RBV, static_cast<int>(value));
            break;
        case CALPULSE_CNT:
            setIntegerParam(GermaniumTPCNT_RBV, static_cast<int>(value));
            break;
        case CALPULSE_MODE:
            setIntegerParam(GermaniumTPENB_RBV, static_cast<int>(value));
            break;
        case MARS_PIPE_DELAY:
            setIntegerParam(GermaniumPLDEL_RBV, static_cast<int>(value));
            break;
        case MARS_RDOUT_ENB:
            setIntegerParam(GermaniumRODEL_RBV, static_cast<int>(value));
            break;
        case TRIG:
            setIntegerParam(GermaniumCNT_RBV, static_cast<int>(value));
            setAcquisitionRunning(value != 0);
            break;
        case COUNT_MODE:
            setIntegerParam(GermaniumMODE, value ? 1 : 0);
            break;
        case EVENT_TIME_CNTR:
            setDoubleParam(GermaniumT, static_cast<double>(value) / 25.0e6);
            break;
        case COUNT_TIME_LO:
            count_time_lo = value;
            break;
        case COUNT_TIME_HI:
        {
            count_time_hi = value;
            double count_time = static_cast<double>( ( static_cast<uint64_t>(count_time_hi ) << 32)
                                                     | count_time_lo );
            setDoubleParam(GermaniumTP, count_time);
            break;
        }
        case UDP_IP_ADDR:
        {
            uint32_t host = ntohl(value);
            struct in_addr a;
            a.s_addr = host;
            char buf[INET_ADDRSTRLEN];
            inet_ntop( AF_INET, &a, buf, sizeof(buf) );
            setStringParam( GermaniumIPADDR_RBV, buf );
            break;
        }
        default:
            break;
    }

    callParamCallbacks();
}
//===========================================================================//

void GermaniumDetector::processReplyRegWrite(uint32_t addr, uint32_t value)
{
    switch (addr)
    {
        case MARS_CALPULSE:
            setIntegerParam(GermaniumTPAMP_RBV, static_cast<int>(value));
            break;
        case CALPULSE_RATE:
            setIntegerParam(GermaniumTPFRQ_RBV, static_cast<int>(value));
            break;
        case CALPULSE_CNT:
            setIntegerParam(GermaniumTPCNT_RBV, static_cast<int>(value));
            break;
        case CALPULSE_MODE:
            setIntegerParam(GermaniumTPENB_RBV, static_cast<int>(value));
            break;
        case MARS_PIPE_DELAY:
            setIntegerParam(GermaniumPLDEL_RBV, static_cast<int>(value));
            break;
        case MARS_RDOUT_ENB:
            setIntegerParam(GermaniumRODEL_RBV, static_cast<int>(value));
            break;
        case DETECTOR_MODEL:
            setIntegerParam(GermaniumDETMODEL, static_cast<int>(value));
            break;
        case VERSIONREG:
            setIntegerParam(GermaniumFVER, static_cast<int>(value));
            break;
        default:
            break;
    }
    callParamCallbacks();
}

//===========================================================================//

void GermaniumDetector::processReplyI2cTempRead( const uint32_t addr, const uint32_t value )
{
    double tempC = static_cast<double>(value >> 4) * 0.0625;
    switch (addr)
    {
        case TEMPERATURE1_SELECTOR: setDoubleParam(GermaniumTEMP1, tempC); break;
        case TEMPERATURE2_SELECTOR: setDoubleParam(GermaniumTEMP2, tempC); break;
        case TEMPERATURE3_SELECTOR: setDoubleParam(GermaniumTEMP3, tempC); break;
    }

    callParamCallbacks();
}

//===========================================================================//

void GermaniumDetector::processReplyXadcRead( const uint32_t addr, const uint32_t value )
{
    (void)addr;
    double tempC = 503.975 * static_cast<double>(value) / 4096.0 - 273.15;
    setDoubleParam(GermaniumZTEMP, tempC);
    callParamCallbacks();
}

//===========================================================================//

void GermaniumDetector::processReplyI2cAdcRead( const uint32_t addr, const uint32_t value )
{
    // LTC2309 12-bit ADC → engineering units
    double raw = static_cast<double>(value);
    switch (addr)
    {
        case ADC_CH_HV_RBV:
            setDoubleParam(GermaniumHV_RBV, raw * 500.0 / 4096.0);
            break;
        case ADC_CH_HV_CUR:
            setDoubleParam(GermaniumHV_CURR, raw * 5.0 / 4096.0);
            break;
        case ADC_CH_P1_CUR:
            setDoubleParam(GermaniumP1_CURR, raw * 500.0 / 4096.0);
            break;
        case ADC_CH_P2_CUR:
            setDoubleParam(GermaniumP2_CURR, raw * 5.0 / 4096.0);
            break;
    }
    callParamCallbacks();
}

//===========================================================================//

void GermaniumDetector::processReplyMarsGlobalSet( const uint32_t addr, const uint32_t value )
{
    (void)addr;
    (void)value;
}

//===========================================================================//

void GermaniumDetector::processReplyMarsGlobalRead( const uint32_t addr, const uint32_t value )
{
    switch ( addr & 0xFFFF )
    {
        case MARS_FIELD_ST:
            setIntegerParam(GermaniumSHPT, static_cast<int>(value));
            break;
        case MARS_FIELD_GAIN:
            setIntegerParam(GermaniumGAIN, static_cast<int>(value));
            break;
        case MARS_FIELD_POL:
            setIntegerParam(GermaniumPOL, static_cast<int>(value));
            break;
        case MARS_FIELD_EBLK:
            setIntegerParam(GermaniumEBLK, static_cast<int>(value));
            break;
        case MARS_FIELD_PUEN:
            setIntegerParam(GermaniumPUEN, static_cast<int>(value));
            break;
        case MARS_FIELD_MFS:
            setIntegerParam(GermaniumMFS, static_cast<int>(value));
            break;
        case MARS_FIELD_TDS:
            setIntegerParam(GermaniumTDS, static_cast<int>(value));
            break;
        case MARS_FIELD_TDM:
            setIntegerParam(GermaniumTDM, static_cast<int>(value));
            break;
        default:
            break;
    }
    callParamCallbacks();
}

//===========================================================================//

void GermaniumDetector::processReplyMarsChannelSet( const uint32_t addr, const uint32_t value )
{
    (void)addr;
    (void)value;
}

//===========================================================================//

void GermaniumDetector::processReplyMarsChannelRead( const uint32_t addr, const uint32_t value )
{
    (void)addr;
    (void)value;
}

//===========================================================================//

void GermaniumDetector::processReplyAdcClkSkewSet( const uint32_t addr, const uint32_t value )
{
    (void)addr;
    (void)value;
}

//===========================================================================//

void GermaniumDetector::processReplyAdcClkSkewRead( const uint32_t addr, const uint32_t value )
{
    switch ( addr )
    {
        case 1: setIntegerParam(GermaniumADC0_CLK_SKEW, static_cast<int>(value)); break;
        case 2: setIntegerParam(GermaniumADC1_CLK_SKEW, static_cast<int>(value)); break;
        case 3: setIntegerParam(GermaniumADC2_CLK_SKEW, static_cast<int>(value)); break;
        default: break;
    }
    callParamCallbacks();
}

//===========================================================================//

void GermaniumDetector::processReplyI2cDacWrite( const uint32_t addr, const uint32_t value )
{
    (void)addr;
    (void)value;
}

//===========================================================================//

void GermaniumDetector::processReplyI2cDacInit( const uint32_t addr, const uint32_t value )
{
    (void)addr;
    (void)value;
}

//===========================================================================//

/*
void GermaniumDetector::zmqDataThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->zmqDataThread();
}
*/

//===========================================================================//

/*
void GermaniumDetector::zmqDataThread()
{
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "[%s]: ZMQ data thread started\n"
             , portName
             );

    while ( threadsRunning.load() )
    {
        // Receive topic frame
        zmq_msg_t topicMsg;
        zmq_msg_init(&topicMsg);
        int rc = zmq_msg_recv(&topicMsg, zmqDataSocket, ZMQ_DONTWAIT);
        if (rc < 0)
        {
            zmq_msg_close(&topicMsg);
            if (errno == EAGAIN)
            {
                // No message available, poll again after brief sleep
                epicsThreadSleep(0.001);
                continue;
            }
            if (errno == ETERM)
                break;
            continue;
        }

        size_t topicSize = zmq_msg_size(&topicMsg);
        char topic[16] = {0};
        size_t copyLen = topicSize < sizeof(topic) - 1 ? topicSize : sizeof(topic) - 1;
        memcpy(topic, zmq_msg_data(&topicMsg), copyLen);
        zmq_msg_close(&topicMsg);

        // Check if there's a second frame (multipart)
        int more = 0;
        size_t moreSize = sizeof(more);
        zmq_getsockopt(zmqDataSocket, ZMQ_RCVMORE, &more, &moreSize);
        if (!more)
            continue;

        // Receive payload frame
        zmq_msg_t payloadMsg;
        zmq_msg_init(&payloadMsg);
        rc = zmq_msg_recv(&payloadMsg, zmqDataSocket, 0);
        if (rc < 0)
        {
            zmq_msg_close(&payloadMsg);
            continue;
        }

        size_t payloadSize = zmq_msg_size(&payloadMsg);
        uint32_t *payloadData = static_cast<uint32_t*>(zmq_msg_data(&payloadMsg));

        if (strncmp(topic, "data", 4) == 0 && acquisitionRunning)
        {
            // Process event data: pairs of uint32_t [event_word, timestamp_word]
            size_t numWords = payloadSize / sizeof(uint32_t);
            for (size_t i = 0; i + 1 < numWords; i += 2)
            {
                uint32_t w1 = payloadData[i];
                uint32_t w2 = payloadData[i + 1];

                int chip    = (w1 >> 27) & 0xF;
                int chan    = (w1 >> 22) & 0x1F;
                int td      = (w1 >> 12) & 0x3FF;
                int pd      = w1 & 0xFFF;

                int element = chip * 32 + chan;
                if (element >= 0 && element < numElements)
                {
                    processPhotonEvent(element, pd, td);
                }
            }

            // Also add raw data to write buffer for file saving
            addDataToWriteBuffer(reinterpret_cast<const uint8_t*>(payloadData),
                                 payloadSize);

            epicsEventSignal(dataAvailable);
        }
        else if (strncmp(topic, "meta", 4) == 0)
        {
            if (payloadSize >= sizeof(uint32_t))
            {
                uint32_t frameNum = payloadData[0];
                setIntegerParam(GermaniumRUNNO, static_cast<int>(frameNum));
                callParamCallbacks();
            }
        }

        zmq_msg_close(&payloadMsg);
    }

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "[%s]: ZMQ data thread stopped\n", portName);
}
*/

//===========================================================================//
