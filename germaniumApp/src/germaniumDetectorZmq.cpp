/**
 * @file germaniumDetectorZmq.cpp
 * @brief ZMQ communication with Zynq C ZMQ server (germ_zserver1).
 *
 * Control plane: ZMQ REQ-REP on port 5555 for register read/write.
 *   Message format: 3 x uint32_t [cmd, addr, value]
 *   cmd=0: read register at addr, returns value in response
 *   cmd=1: write value to register at addr
 *
 * Data plane: ZMQ SUB on port 5556 for event data.
 *   Topics: "data" (raw event words), "meta" (frame number)
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "germaniumDetector.hpp"
#include <cstring>
#include <cstdio>
#include <arpa/inet.h>

//===========================================================================//

/*
 * Initialize ZMQ context and sockets for communication with Zynq.
 */
bool germaniumDetector::initializeZmq()
{
    printf("Germanium: Initializing ZMQ connection to %s...\n", ipAddress);

    // Create ZMQ context
    zmqContext = zmq_ctx_new();
    if (!zmqContext)
    {
        printf("Germanium: Failed to create ZMQ context\n");
        return false;
    }

    // --- Control socket (REQ-REP) ---
    zmqControlSocket = zmq_socket(zmqContext, ZMQ_REQ);
    if (!zmqControlSocket)
    {
        printf("Germanium: Failed to create ZMQ REQ socket\n");
        zmq_ctx_destroy(zmqContext);
        zmqContext = nullptr;
        return false;
    }

    // Set timeouts to avoid blocking forever
    int timeout_ms = 2000;  // 2 second timeout
    zmq_setsockopt(zmqControlSocket, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    zmq_setsockopt(zmqControlSocket, ZMQ_SNDTIMEO, &timeout_ms, sizeof(timeout_ms));

    // Set linger to 0 so zmq_close doesn't block
    int linger = 0;
    zmq_setsockopt(zmqControlSocket, ZMQ_LINGER, &linger, sizeof(linger));

    char controlEndpoint[128];
    snprintf(controlEndpoint, sizeof(controlEndpoint),
             "tcp://%s:%d", ipAddress, ZMQ_CONTROL_PORT);

    if (zmq_connect(zmqControlSocket, controlEndpoint) != 0)
    {
        printf("Germanium: Failed to connect ZMQ REQ to %s: %s\n",
               controlEndpoint, zmq_strerror(errno));
        zmq_close(zmqControlSocket);
        zmq_ctx_destroy(zmqContext);
        zmqControlSocket = nullptr;
        zmqContext = nullptr;
        return false;
    }

    // --- Data socket (SUB) ---
    zmqDataSocket = zmq_socket(zmqContext, ZMQ_SUB);
    if (!zmqDataSocket)
    {
        printf("Germanium: Failed to create ZMQ SUB socket\n");
        zmq_close(zmqControlSocket);
        zmq_ctx_destroy(zmqContext);
        zmqControlSocket = nullptr;
        zmqContext = nullptr;
        return false;
    }

    zmq_setsockopt(zmqDataSocket, ZMQ_LINGER, &linger, sizeof(linger));

    // Subscribe to "data" and "meta" topics
    zmq_setsockopt(zmqDataSocket, ZMQ_SUBSCRIBE, "data", 4);
    zmq_setsockopt(zmqDataSocket, ZMQ_SUBSCRIBE, "meta", 4);

    char dataEndpoint[128];
    snprintf(dataEndpoint, sizeof(dataEndpoint),
             "tcp://%s:%d", ipAddress, ZMQ_DATA_PORT);

    if (zmq_connect(zmqDataSocket, dataEndpoint) != 0)
    {
        printf("Germanium: Failed to connect ZMQ SUB to %s: %s\n",
               dataEndpoint, zmq_strerror(errno));
        zmq_close(zmqDataSocket);
        zmq_close(zmqControlSocket);
        zmq_ctx_destroy(zmqContext);
        zmqDataSocket = nullptr;
        zmqControlSocket = nullptr;
        zmqContext = nullptr;
        return false;
    }

    // Create mutex for control socket access
    zmqMutex = epicsMutexCreate();

    zmqInitialized = true;

    // Verify the server supports the MARS delta-config protocol.
    // Send a no-op CMD_MARS_LOAD (chip_mask=0).
    {
        ZmqCommandMsg probe;
        probe.cmd   = ZMQ_CMD_MARS_LOAD;
        probe.addr  = 0;           // chip_mask=0 → no-op
        probe.value = 0;

        ZmqCommandMsg reply;
        if (zmqSendRecv(probe, &reply) != asynSuccess
            || reply.cmd != ZMQ_CMD_MARS_LOAD)
        {
            printf("Germanium: ERROR — server does not support delta-config protocol. "
                   "Upgrade the Zynq ZMQ server.\n");
        }
        else
        {
            printf("Germanium: delta-config protocol verified\n");
        }
    }

    printf("Germanium: ZMQ initialized - Control: %s, Data: %s\n",
           controlEndpoint, dataEndpoint);
    return true;
}

//===========================================================================//

/*
 * Close ZMQ sockets and context.
 */
void germaniumDetector::closeZmq()
{
    zmqInitialized = false;

    if (zmqDataSocket)
    {
        zmq_close(zmqDataSocket);
        zmqDataSocket = nullptr;
    }

    if (zmqControlSocket)
    {
        zmq_close(zmqControlSocket);
        zmqControlSocket = nullptr;
    }

    if (zmqContext)
    {
        zmq_ctx_destroy(zmqContext);
        zmqContext = nullptr;
    }

    if (zmqMutex)
    {
        epicsMutexDestroy(zmqMutex);
        zmqMutex = nullptr;
    }

    printf("Germanium: ZMQ connections closed\n");
}

//===========================================================================//
// ZMQ message Tx/Rx wrappers.
//
int germaniumDetector::zmqTx( void* socket, ZmqCommandMsg* msg, size_t len, int flags)
{
    printf("[%s]: cmd=0x%02X addr=0x%04X value=0x%08X\n\n",
           __func__, msg->cmd, msg->addr, msg->value);
    return zmq_send( socket, msg, len, flags );
}

int germaniumDetector::zmqRx( void* socket, ZmqCommandMsg* msg, size_t len, int flags)
{
    int rc = zmq_recv(socket, msg, len, flags);
    printf("[%s]: cmd=0x%02X addr=0x%04X value=0x%08X\n",
           __func__, msg->cmd, msg->addr, msg->value);
    return rc;
}

//===========================================================================//
/*
 * Write a value to an FPGA register via ZMQ REQ-REP.
 *
 * Sends: [CMD_REG_WRITE, addr, value] (3 x uint32_t = 12 bytes)
 * Receives: [CMD_REG_WRITE, addr, value] echoed back
 */
asynStatus germaniumDetector::zmqRegisterWrite(uint32_t addr, uint32_t value)
{
    if (!zmqInitialized)
    {
        printf("Germanium: ZMQ not initialized\n");
        return asynError;
    }

    ZmqCommandMsg msg;
    msg.cmd   = ZMQ_CMD_REG_WRITE;
    msg.addr  = addr;
    msg.value = value;

    printf("[%s]: cmd=0x%02X addr=0x%04X value=0x%08X\n",
           __func__, msg.cmd, msg.addr, msg.value);

    epicsMutexLock(zmqMutex);

    // Send command
    int rc = zmqTx(zmqControlSocket, &msg, sizeof(msg), 0);
    if (rc != sizeof(msg))
    {
        printf("Germanium: ZMQ send failed for write reg %u: %s\n",
               addr, zmq_strerror(errno));
        epicsMutexUnlock(zmqMutex);
        return asynError;
    }

    // Wait for reply (REQ-REP pattern requires recv after send)
    ZmqCommandMsg reply;
    rc = zmqRx(zmqControlSocket, &reply, sizeof(reply), 0);
    if (rc != sizeof(reply))
    {
        printf("Germanium: ZMQ recv failed for write reg %u: %s\n",
               addr, zmq_strerror(errno));
        epicsMutexUnlock(zmqMutex);
        return asynError;
    }

    epicsMutexUnlock(zmqMutex);

    // Update readback (RBV) parameters from the echoed reply
    updateRbvFromReply(addr, reply.value);

    return asynSuccess;
}

//===========================================================================//

/*
 * Update readback PVs based on echoed register write replies.
 */
void germaniumDetector::updateRbvFromReply(uint32_t addr, uint32_t value)
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
        case DETECTOR_TYPE:
            setIntegerParam(GermaniumDETTYPE, static_cast<int>(value));
            break;
        case VERSIONREG:
            setIntegerParam(GermaniumVER, static_cast<int>(value));
            break;
        case UDP_IP_ADDR:
        {
            uint32_t host = ntohl(value);
            struct in_addr a;
            a.s_addr = htonl(host);
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &a, buf, sizeof(buf));
            setStringParam(GermaniumIPADDR_RBV, buf);
            break;
        }
        default:
            break;
    }
    callParamCallbacks();
}

//===========================================================================//

/*
 * Sends: [CMD_REG_READ, addr, 0] (3 x uint32_t = 12 bytes)
 * Receives: [CMD_REG_READ, addr, value] with value filled in
 */
asynStatus germaniumDetector::zmqRegisterRead(uint32_t addr, uint32_t *value)
{
    if (!zmqInitialized)
    {
        printf("Germanium: ZMQ not initialized\n");
        return asynError;
    }

    ZmqCommandMsg msg;
    msg.cmd   = ZMQ_CMD_REG_READ;
    msg.addr  = addr;
    msg.value = 0;

    epicsMutexLock(zmqMutex);

    // Send command
    int rc = zmqTx(zmqControlSocket, &msg, sizeof(msg), 0);
    if (rc != sizeof(msg))
    {
        printf("Germanium: ZMQ send failed for read reg %u: %s\n",
               addr, zmq_strerror(errno));
        epicsMutexUnlock(zmqMutex);
        return asynError;
    }

    // Wait for reply
    ZmqCommandMsg reply;
    rc = zmqRx(zmqControlSocket, &reply, sizeof(reply), 0);
    if (rc != sizeof(reply))
    {
        printf("Germanium: ZMQ recv failed for read reg %u: %s\n",
               addr, zmq_strerror(errno));
        epicsMutexUnlock(zmqMutex);
        return asynError;
    }

    epicsMutexUnlock(zmqMutex);

    *value = reply.value;
    return asynSuccess;
}

//===========================================================================//

/*
 * Common helper: send a ZmqCommandMsg and receive the reply.
 * Caller must NOT hold zmqMutex — this function locks it internally.
 */
asynStatus germaniumDetector::zmqSendRecv(ZmqCommandMsg &msg, ZmqCommandMsg *reply)
{
    if (!zmqInitialized) return asynError;

    epicsMutexLock(zmqMutex);

    int rc = zmqTx(zmqControlSocket, &msg, sizeof(msg), 0);
    if (rc != sizeof(msg))
    {
        epicsMutexUnlock(zmqMutex);
        return asynError;
    }

    rc = zmqRx(zmqControlSocket, reply, sizeof(*reply), 0);
    if (rc != sizeof(*reply))
    {
        epicsMutexUnlock(zmqMutex);
        return asynError;
    }

    epicsMutexUnlock(zmqMutex);
    return asynSuccess;
}

//===========================================================================//

/*
 * Set a single global config field on selected chips.
 * One ZMQ round-trip regardless of chip count.
 *
 * Message: [CMD_MARS_SET_GLOBAL, chip_mask<<16 | field_id, value]
 */
asynStatus germaniumDetector::zmqMarsSetGlobal(uint32_t chipMask, MarsGlobalField field,
                                               uint32_t value)
{
    ZmqCommandMsg msg;
    msg.cmd   = ZMQ_CMD_MARS_SET_GLOBAL;
    msg.addr  = (chipMask << 16) | static_cast<uint32_t>(field);
    msg.value = value;

    printf("[%s]: cmd=0x%02X addr=0x%04X value=0x%08X\n\n",
           __func__, msg.cmd, msg.addr, msg.value);

    ZmqCommandMsg reply;
    return zmqSendRecv(msg, &reply);
}

//===========================================================================//

/*
 * Set a single per-channel config field.
 * channel = 0..383 for a specific channel, or 0xFFF for all.
 *
 * Message: [CMD_MARS_SET_CHANNEL, channel<<16 | field_id, value]
 */
asynStatus germaniumDetector::zmqMarsSetChannel(uint32_t channel, MarsChannelField field,
                                                uint32_t value)
{
    ZmqCommandMsg msg;
    msg.cmd   = ZMQ_CMD_MARS_SET_CHANNEL;
    msg.addr  = (channel << 16) | static_cast<uint32_t>(field);
    msg.value = value;

    ZmqCommandMsg reply;
    return zmqSendRecv(msg, &reply);
}

//===========================================================================//

/*
 * Trigger MARS config pack + register-write on Zynq for selected chips.
 * One ZMQ round-trip triggers the full stuff_mars sequence locally on Zynq.
 *
 * Message: [CMD_MARS_LOAD, chip_mask, 0]
 */
asynStatus germaniumDetector::zmqMarsLoad(uint32_t chipMask)
{
    ZmqCommandMsg msg;
    msg.cmd   = ZMQ_CMD_MARS_LOAD;
    msg.addr  = chipMask;
    msg.value = 0;

    ZmqCommandMsg reply;
    return zmqSendRecv(msg, &reply);
}

//===========================================================================//

/*
 * ZMQ data reception thread.
 * Subscribes to "data" and "meta" topics on port 5556.
 *
 * "data" messages: multipart [topic, payload]
 *   payload = raw uint32_t event words (pairs: event_data + timestamp)
 *
 * "meta" messages: multipart [topic, payload]
 *   payload = uint32_t frame_number
 */
void germaniumDetector::zmqDataThreadC(void *pPvt)
{
    static_cast<germaniumDetector*>(pPvt)->zmqDataThread();
}

void germaniumDetector::zmqDataThread()
{
    printf("Germanium: ZMQ data thread started\n");

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

    printf("Germanium: ZMQ data thread stopped\n");
}

//===========================================================================//
