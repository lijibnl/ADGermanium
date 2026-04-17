/**
 * @file GermaniumDetectorZmq.cpp
 * @brief Async ZMQ communication with ZynqDetector (PUSH-PULL).
 *
 * Command channel: IOC PUSH → Zynq PULL on port 5555
 * Reply channel:   Zynq PUSH → IOC PULL on port 5557
 * Data channel:    Zynq PUB  → IOC SUB  on port 5556 (unchanged)
 *
 * Threads:
 *   Tx thread:         drains txQueue_, sends via tx socket
 *   Control Rx thread: receives from PULL socket, updates PV cache
 *   Data Rx thread:    receives from SUB socket (event data)
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "GermaniumDetector.hpp"
#include "GermaniumDetectorParamFormat.hpp"
#include <cstring>
#include <cstdio>
#include <arpa/inet.h>

//===========================================================================//

bool GermaniumDetector::initializeZmq()
{
    printf("%s: initializing async ZMQ to %s\n", __func__, ipAddress);

    zmqContext = zmq_ctx_new();
    if (!zmqContext)
    {
        printf("%s: failed to create ZMQ context\n", __func__);
        return false;
    }

    int linger = 0;

    //--------------------------------------------------------------
    // Tx socket — sends commands to Zynq
    //--------------------------------------------------------------
    zmqTxSocket = zmq_socket(zmqContext, ZMQ_PUSH);
    if (!zmqTxSocket)
    {
        printf("%s: failed to create tx socket\n", __func__);
        zmq_ctx_destroy(zmqContext);
        zmqContext = nullptr;
        return false;
    }

    zmq_setsockopt(zmqTxSocket, ZMQ_LINGER, &linger, sizeof(linger));

    char txEndpoint[128];
    snprintf(txEndpoint, sizeof(txEndpoint),
             "tcp://%s:%d", ipAddress, ZMQ_CMD_PORT);

    if (zmq_connect(zmqTxSocket, txEndpoint) != 0)
    {
        printf("%s: failed to connect tx to %s: %s\n", __func__, txEndpoint, zmq_strerror(errno));
        zmq_close(zmqTxSocket); zmqTxSocket = nullptr;
        zmq_ctx_destroy(zmqContext); zmqContext = nullptr;
        return false;
    }

    //--------------------------------------------------------------
    // Rx socket — receives replies from Zynq
    //--------------------------------------------------------------
    zmqRxSocket = zmq_socket(zmqContext, ZMQ_PULL);
    if (!zmqRxSocket)
    {
        printf("%s: failed to create rx socket\n", __func__);
        zmq_close(zmqTxSocket); zmqTxSocket = nullptr;
        zmq_ctx_destroy(zmqContext); zmqContext = nullptr;
        return false;
    }

    zmq_setsockopt(zmqRxSocket, ZMQ_LINGER, &linger, sizeof(linger));

    char rxEndpoint[128];
    snprintf(rxEndpoint, sizeof(rxEndpoint),
             "tcp://%s:%d", ipAddress, ZMQ_REPLY_PORT);

    if (zmq_connect(zmqRxSocket, rxEndpoint) != 0)
    {
        printf("%s: failed to connect rx to %s: %s\n", __func__, rxEndpoint, zmq_strerror(errno));
        zmq_close(zmqRxSocket); zmqRxSocket = nullptr;
        zmq_close(zmqTxSocket); zmqTxSocket = nullptr;
        zmq_ctx_destroy(zmqContext); zmqContext = nullptr;
        return false;
    }

    //--------------------------------------------------------------
    // SUB socket — event data from Zynq (unchanged)
    //--------------------------------------------------------------
    zmqDataSocket = zmq_socket(zmqContext, ZMQ_SUB);
    if (!zmqDataSocket)
    {
        printf("%s: failed to create SUB socket\n", __func__);
        zmq_close(zmqRxSocket); zmqRxSocket = nullptr;
        zmq_close(zmqTxSocket); zmqTxSocket = nullptr;
        zmq_ctx_destroy(zmqContext); zmqContext = nullptr;
        return false;
    }

    zmq_setsockopt(zmqDataSocket, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_setsockopt(zmqDataSocket, ZMQ_SUBSCRIBE, "data", 4);
    zmq_setsockopt(zmqDataSocket, ZMQ_SUBSCRIBE, "meta", 4);

    char dataEndpoint[128];
    snprintf(dataEndpoint, sizeof(dataEndpoint),
             "tcp://%s:%d", ipAddress, ZMQ_DATA_PORT);

    if (zmq_connect(zmqDataSocket, dataEndpoint) != 0)
    {
        printf("%s: failed to connect SUB to %s: %s\n", __func__, dataEndpoint, zmq_strerror(errno));
        zmq_close(zmqDataSocket); zmqDataSocket = nullptr;
        zmq_close(zmqRxSocket); zmqRxSocket = nullptr;
        zmq_close(zmqTxSocket); zmqTxSocket = nullptr;
        zmq_ctx_destroy(zmqContext); zmqContext = nullptr;
        return false;
    }

    //--------------------------------------------------------------
    // Tx queue infrastructure
    //--------------------------------------------------------------
    txQueueMutex_ = epicsMutexCreate();
    txQueueEvent_ = epicsEventCreate(epicsEventEmpty);

    zmqInitialized = true;

    //--------------------------------------------------------------
    // Start Tx and Control Rx threads
    //--------------------------------------------------------------
    zmqTxThreadId = epicsThreadCreate("zmqTx",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        zmqTxThreadC, this);

    if (!zmqTxThreadId)
    {
        printf("%s: failed to create Tx thread\n", __func__);
        closeZmq();
        return false;
    }
    printf("%s: Tx thread started\n", __func__);

    zmqControlRxThreadId = epicsThreadCreate("zmqControlRx",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        zmqControlRxThreadC, this);

    if (!zmqControlRxThreadId)
    {
        printf("%s: failed to create Control Rx thread\n", __func__);
        closeZmq();
        return false;
    }
    printf("%s: Control Rx threads started\n", __func__);

    //--------------------------------------------------------------
    // Send initialization commands
    //--------------------------------------------------------------
    //zmqTx(ZMQ_CMD_I2C_DAC_INIT, 0, 0);

    //--------------------------------------------------------------
    // Read startup PVs so readback values are populated immediately
    //--------------------------------------------------------------
    zmqTx(ZMQ_CMD_REG_READ, VERSIONREG, 0);
    zmqTx(ZMQ_CMD_REG_READ, DETECTOR_MODEL, 0);
    //zmqTx(ZMQ_CMD_REG_READ, MARS_PIPE_DELAY, 0);
    //zmqTx(ZMQ_CMD_REG_READ, MARS_RDOUT_ENB, 0);
    //zmqTx(ZMQ_CMD_REG_READ, MARS_CALPULSE, 0);
    //zmqTx(ZMQ_CMD_REG_READ, CALPULSE_RATE, 0);
    //zmqTx(ZMQ_CMD_REG_READ, CALPULSE_CNT, 0);
    //zmqTx(ZMQ_CMD_REG_READ, CALPULSE_MODE, 0);
    //zmqTx(ZMQ_CMD_REG_READ, COUNT_MODE, 0);

    printf("%s: ZMQ initialized\n", __func__);
    printf("  - Tx: %s\n", txEndpoint);
    printf("  - Rx: %s\n", rxEndpoint);
    printf("  - SUB: %s\n", dataEndpoint);
    return true;
}

//===========================================================================//

void GermaniumDetector::closeZmq()
{
    zmqInitialized = false;

    if (zmqDataSocket)  { zmq_close(zmqDataSocket);  zmqDataSocket = nullptr;  }
    if (zmqRxSocket)  { zmq_close(zmqRxSocket);  zmqRxSocket = nullptr;  }
    if (zmqTxSocket)  { zmq_close(zmqTxSocket);  zmqTxSocket = nullptr;  }

    if (zmqContext)      { zmq_ctx_destroy(zmqContext); zmqContext = nullptr;    }

    if (txQueueMutex_) { epicsMutexDestroy(txQueueMutex_); txQueueMutex_ = nullptr; }
    if (txQueueEvent_) { epicsEventDestroy(txQueueEvent_); txQueueEvent_ = nullptr; }

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s: ZMQ connections closed\n", portName);
}

//===========================================================================//

asynStatus GermaniumDetector::zmqTx(uint32_t cmd, uint32_t addr, uint32_t value)
{
    if (!zmqInitialized)
    {
        printf("%s: attempt to send ZMQ command before initialization\n", __func__);
        return asynError;
    }

    TxQueueItem item;
    item.msg.cmd   = cmd;
    item.msg.addr  = addr;
    item.msg.value = value;

    epicsMutexLock(txQueueMutex_);
    printf("%s: queuing ZMQ command: cmd=0x%02X addr=0x%04X value=0x%08X\n", __func__, cmd, addr, value);
    printf("%s: queuing ZMQ command (decoded): %s\n", __func__, format_zmq_msg(ZmqCommandMsg{cmd, addr, value}).c_str());
    txQueue_.push_back(item);
    epicsMutexUnlock(txQueueMutex_);

    epicsEventSignal(txQueueEvent_);
    return asynSuccess;
}

//===========================================================================//

void GermaniumDetector::zmqSend(const ZmqCommandMsg& msg)
{
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
        "%s: ZMQ TX: cmd=0x%02X addr=0x%04X value=0x%08X\n",
        portName, msg.cmd, msg.addr, msg.value);
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
        "%s: ZMQ TX (decoded): %s\n",
        portName, format_zmq_msg(msg).c_str());
    zmq_send(zmqTxSocket, &msg, sizeof(msg), 0);
}

//===========================================================================//

asynStatus GermaniumDetector::zmqMarsSetGlobal(uint32_t chipMask,
                                               MarsGlobalField field,
                                               uint32_t value)
{
    return zmqTx(ZMQ_CMD_MARS_SET_GLOBAL,
                        (chipMask << 16) | static_cast<uint32_t>(field),
                        value);
}

//===========================================================================//

asynStatus GermaniumDetector::zmqMarsSetChannel(uint32_t channel,
                                                MarsChannelField field,
                                                uint32_t value)
{
    return zmqTx(ZMQ_CMD_MARS_SET_CHANNEL,
                        (channel << 16) | static_cast<uint32_t>(field),
                        value);
}

//===========================================================================//

asynStatus GermaniumDetector::zmqMarsLoad(uint32_t chipMask)
{
    return zmqTx(ZMQ_CMD_MARS_LOAD, chipMask, 0);
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
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s: Tx thread started\n", portName);

    while (threadsRunning)
    {
        epicsEventWaitWithTimeout(txQueueEvent_, 0.1);

        // Drain the queue
        std::vector<TxQueueItem> batch;
        epicsMutexLock(txQueueMutex_);
        batch.swap(txQueue_);
        epicsMutexUnlock(txQueueMutex_);

        for (auto& item : batch)
        {
            printf("%s: sending ZMQ command from Tx thread: cmd=0x%02X addr=0x%04X value=0x%08X\n", __func__, item.msg.cmd, item.msg.addr, item.msg.value);
            printf("%s: sending ZMQ command from Tx thread (decoded): %s\n", __func__, format_zmq_msg(item.msg).c_str());
            zmqSend(item.msg);
        }
    }

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s: Tx thread stopped\n", portName);
}

//===========================================================================//
//  Control Rx thread: receives replies, updates PV cache
//===========================================================================//

void GermaniumDetector::zmqControlRxThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->zmqControlRxThread();
}

void GermaniumDetector::zmqControlRxThread()
{
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s: Control Rx thread started\n", portName);
    printf("%s: Control Rx thread started\n", __func__);

    while (threadsRunning)
    {
        ZmqCommandMsg reply;
        int rc = zmq_recv(zmqRxSocket, &reply, sizeof(reply), ZMQ_DONTWAIT);
        if (rc < 0)
        {
            if (errno == EAGAIN)
            {
                epicsThreadSleep(0.001);
                continue;
            }
            if (errno == ETERM)
                break;
            continue;
        }

        if (rc != sizeof(ZmqCommandMsg))
            continue;

        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s: ZMQ RX: cmd=0x%02X addr=0x%04X value=0x%08X\n",
            portName, reply.cmd, reply.addr, reply.value);
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s: ZMQ RX (decoded): %s\n",
            portName, format_zmq_msg(reply).c_str());

        processReply(reply);
    }

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s: Control Rx thread stopped\n", portName);
}

//===========================================================================//

void GermaniumDetector::processReply(const ZmqCommandMsg& reply)
{
    switch (reply.cmd)
    {
        case ZMQ_CMD_REG_READ:
        {
            uint32_t addr  = reply.addr;
            uint32_t value = reply.value;

            // Update the appropriate readback parameter
            if (addr == VERSIONREG)
                setIntegerParam(GermaniumFVER, static_cast<int>(value));
            else if (addr == DETECTOR_MODEL)
                setIntegerParam(GermaniumDETMODEL, static_cast<int>(value));
            else if (addr == MARS_CALPULSE)
                setIntegerParam(GermaniumTPAMP_RBV, static_cast<int>(value));
            else if (addr == CALPULSE_RATE)
                setIntegerParam(GermaniumTPFRQ_RBV, static_cast<int>(value));
            else if (addr == CALPULSE_CNT)
                setIntegerParam(GermaniumTPCNT_RBV, static_cast<int>(value));
            else if (addr == CALPULSE_MODE)
                setIntegerParam(GermaniumTPENB_RBV, static_cast<int>(value));
            else if (addr == MARS_PIPE_DELAY)
                setIntegerParam(GermaniumPLDEL_RBV, static_cast<int>(value));
            else if (addr == MARS_RDOUT_ENB)
                setIntegerParam(GermaniumRODEL_RBV, static_cast<int>(value));
            else if (addr == TRIG)
                setIntegerParam(GermaniumCNT_RBV, static_cast<int>(value));
            else if (addr == COUNT_MODE)
                setIntegerParam(GermaniumMODE, value ? 1 : 0);
            else if (addr == EVENT_TIME_CNTR)
                setDoubleParam(GermaniumT, static_cast<double>(value) / 25.0e6);
            else if (addr == COUNT_TIME_LO || addr == COUNT_TIME_HI)
            {
                // Time preset readback handled below
            }
            else if (addr == UDP_IP_ADDR)
            {
                uint32_t host = ntohl(value);
                struct in_addr a;
                a.s_addr = host;
                char buf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &a, buf, sizeof(buf));
                setStringParam(GermaniumIPADDR_RBV, buf);
                asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s: Received IP address readback: %s\n", portName, buf);
            }
            callParamCallbacks();
            break;
        }

        case ZMQ_CMD_REG_WRITE:
            updateRbvFromReply(reply.addr, reply.value);
            break;

        case ZMQ_CMD_I2C_TEMP_READ:
        {
            double tempC = static_cast<double>(reply.value >> 4) * 0.0625;
            switch (reply.addr)
            {
                case 0: setDoubleParam(GermaniumTEMP1, tempC); break;
                case 1: setDoubleParam(GermaniumTEMP2, tempC); break;
                case 2: setDoubleParam(GermaniumTEMP3, tempC); break;
            }
            break;
        }

        case ZMQ_CMD_XADC_READ:
        {
            double tempC = 503.975 * static_cast<double>(reply.value) / 4096.0 - 273.15;
            setDoubleParam(GermaniumZTEMP, tempC);
            break;
        }

        case ZMQ_CMD_I2C_ADC_READ:
        {
            // LTC2309 12-bit ADC → engineering units
            double raw = static_cast<double>(reply.value);
            switch (reply.addr)
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
            break;
        }

        case ZMQ_CMD_MARS_SET_GLOBAL:
        case ZMQ_CMD_MARS_SET_CHANNEL:
        case ZMQ_CMD_MARS_LOAD:
        case ZMQ_CMD_ADC_CLK_SKEW:
        case ZMQ_CMD_I2C_DAC_WRITE:
        case ZMQ_CMD_I2C_DAC_INIT:
            // Confirmation only — no PV update needed
            break;

        default:
            break;
    }

    callParamCallbacks();
}

//===========================================================================//

void GermaniumDetector::updateRbvFromReply(uint32_t addr, uint32_t value)
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
void GermaniumDetector::zmqDataThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->zmqDataThread();
}

void GermaniumDetector::zmqDataThread()
{
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s: ZMQ data thread started\n", portName);

    while (threadsRunning)
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

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s: ZMQ data thread stopped\n", portName);
}

//===========================================================================//
