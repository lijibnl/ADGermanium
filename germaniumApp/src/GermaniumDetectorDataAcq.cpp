/**
 * @file GermaniumDetectorDataAcq.cpp
 * @brief PL UDP data reception, file writing, and data processing threads.
 *
 * Receives raw detector events from the PL UDP interface (port 32003),
 * processes events into MCA/TDC spectra, and writes data to segmented
 * binary files.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/03/2026
 * 
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "GermaniumDetector.hpp"
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <thread>
#include <print>

//===========================================================================//

/*
 * Initialize PL UDP socket for raw data from FPGA.
 * This is the dedicated PL interface (port 32003), independent of ZMQ.
 */
bool GermaniumDetector::initializePlUdpSocket()
{
    plUdpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (plUdpSocket < 0)
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "%s: failed to create PL UDP socket: %s\n"
                 , portName
                 , strerror(errno)
                 );
        return false;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(plUdpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Increase receive buffer
    int rcvbuf = 1024 * 1024;  // 1MB
    setsockopt(plUdpSocket, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port = htons(PL_UDP_DATA_PORT);

    if (bind(plUdpSocket, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) < 0)
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "%s: failed to bind PL UDP socket to port %d: %s\n"
                 , portName
                 , PL_UDP_DATA_PORT
                 , strerror(errno)
                 );
        close(plUdpSocket);
        plUdpSocket = -1;
        return false;
    }

    plUdpInitialized = true;
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: PL UDP socket bound to port %d\n"
             , portName
             , PL_UDP_DATA_PORT
             );
    return true;
}

//===========================================================================//

void GermaniumDetector::closePlUdpSocket()
{
    plUdpInitialized = false;
    if (plUdpSocket >= 0)
    {
        close(plUdpSocket);
        plUdpSocket = -1;
    }
}

//===========================================================================//

void GermaniumDetector::allocateDataArrays()
{
    // Flat atomic arrays — safe for concurrent access from multiple
    // producer threads (zmqData, plUdp) and the EPICS read thread.
    size_t mcaTotal = static_cast<size_t>(numElements) * SPECTRUM_SIZE;
    size_t tdcTotal = static_cast<size_t>(numElements) * TDC_SIZE;

    //mcaData    = new std::atomic<uint32_t>[mcaTotal];
    //tdcData    = new std::atomic<uint32_t>[tdcTotal];
    //countRates = new std::atomic<uint32_t>[numElements];
    //totalCounts= new std::atomic<uint64_t>[numElements];
    mcaData    = std::make_unique<std::atomic<uint32_t>[]>(mcaTotal);
    tdcData    = std::make_unique<std::atomic<uint32_t>[]>(tdcTotal);
    countRates = std::make_unique<std::atomic<uint32_t>[]>(numElements);
    totalCounts= std::make_unique<std::atomic<uint64_t>[]>(numElements);

    for (size_t i = 0; i < mcaTotal; i++)
        mcaData[i].store(0, std::memory_order_relaxed);
    for (size_t i = 0; i < tdcTotal; i++)
        tdcData[i].store(0, std::memory_order_relaxed);
    for (int i = 0; i < numElements; i++)
    {
        countRates[i].store(0, std::memory_order_relaxed);
        totalCounts[i].store(0, std::memory_order_relaxed);
    }
    evttot.store(0, std::memory_order_relaxed);

    // Allocate lock-free block queue
    //dataQueue = new DataBlock[DATA_QUEUE_CAPACITY];
    dataQueue = std::make_unique<DataBlock[]>(DATA_QUEUE_CAPACITY);
    for (int i = 0; i < DATA_QUEUE_CAPACITY; i++)
        dataQueue[i].state.store(DATA_BLOCK_FREE, std::memory_order_relaxed);

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s: allocated data arrays for %d elements\n", portName, numElements);
}

//===========================================================================//

void GermaniumDetector::processPhotonEvent(int element, int energy, int tdValue)
{
    if (element < 0 || element >= numElements) return;
    if (energy < 0 || energy >= SPECTRUM_SIZE) return;
    if (tdValue < 0 || tdValue >= TDC_SIZE) return;

    mcaData[element * SPECTRUM_SIZE + energy].fetch_add(1, std::memory_order_relaxed);
    tdcData[element * TDC_SIZE + tdValue].fetch_add(1, std::memory_order_relaxed);
    countRates[element].fetch_add(1, std::memory_order_relaxed);
    totalCounts[element].fetch_add(1, std::memory_order_relaxed);
    evttot.fetch_add(1, std::memory_order_relaxed);
}

//===========================================================================//

void GermaniumDetector::clearSpectra()
{
    size_t mcaTotal = static_cast<size_t>(numElements) * SPECTRUM_SIZE;
    size_t tdcTotal = static_cast<size_t>(numElements) * TDC_SIZE;

    for (size_t i = 0; i < mcaTotal; i++)
        mcaData[i].store(0, std::memory_order_relaxed);
    for (size_t i = 0; i < tdcTotal; i++)
        tdcData[i].store(0, std::memory_order_relaxed);
    for (int i = 0; i < numElements; i++)
    {
        countRates[i].store(0, std::memory_order_relaxed);
        totalCounts[i].store(0, std::memory_order_relaxed);
    }
    evttot.store(0, std::memory_order_relaxed);
}

//===========================================================================//

/*
 * PL UDP data reception thread.
 *
 * Receives raw packets from FPGA PL interface. Packet format:
 *   [packet_counter][SOF_MARKER][frame_num][reserved]
 *   [event_data][timestamp] ... (repeated)
 *   [num_lost_events][EOF_MARKER]
 */
void GermaniumDetector::plUdpDataThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->plUdpDataThread();
}

void GermaniumDetector::plUdpDataThread()
{
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: PL UDP data thread started\n"
             , portName
             );

    uint8_t recvBuf[UDP_BUFFER_SIZE];
    struct sockaddr_in senderAddr;
    socklen_t addrLen = sizeof(senderAddr);

    while ( threadsRunning.load() )
    {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(plUdpSocket, &readfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int result = select(plUdpSocket + 1, &readfds, nullptr, nullptr, &timeout);
        if (result <= 0) continue;

        ssize_t bytesReceived = recvfrom( plUdpSocket
                                        , recvBuf
                                        , sizeof(recvBuf)
                                        , 0
                                        , (struct sockaddr*)&senderAddr
                                        , &addrLen
                                        );
        if (bytesReceived <= 0) continue;

        if (!acquisitionRunning.load()) continue;

        // Parse packet as big-endian 32-bit words
        size_t numWords = bytesReceived / sizeof(uint32_t);
        uint32_t *words = reinterpret_cast<uint32_t*>(recvBuf);

        // Process event data words (skip headers, detect SOF/EOF markers)
        for (size_t i = 0; i + 1 < numWords; i += 2)
        {
            uint32_t w1 = ntohl(words[i]);
            uint32_t w2 = ntohl(words[i + 1]);

            // Skip markers
            if (w1 == SOF_MARKER || w1 == EOF_MARKER) continue;
            if (w2 == SOF_MARKER || w2 == EOF_MARKER) continue;

            // Only process if w2 looks like a timestamp (bit 31 set)
            if (!(w2 & 0x80000000)) continue;

            int chip = (w1 >> 27) & 0xF;
            int chan  = (w1 >> 22) & 0x1F;
            int td   = (w1 >> 12) & 0x3FF;
            int pd   = w1 & 0xFFF;

            int element = chip * 32 + chan;
            if (element >= 0 && element < numElements)
                processPhotonEvent(element, pd, td);
        }

        // Add raw data to write buffer
        addDataToWriteBuffer(recvBuf, bytesReceived);
        epicsEventSignal(dataAvailable);
    }

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: PL UDP data thread stopped\n"
             , portName
             );
}

//===========================================================================//

void GermaniumDetector::dataProcessingThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->dataProcessingThread();
}

void GermaniumDetector::dataProcessingThread()
{
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: Data processing thread started\n"
             , portName
             );

    int arrayCounter = 0;
    int colorMode = NDColorModeMono;

    while ( threadsRunning.load() )
    {
        epicsEventWaitWithTimeout(dataAvailable, 1.0);

        const bool running = acquisitionRunning.load();
        setIntegerParam(GermaniumSS, running ? 1 : 0);
        callParamCallbacks();

        // Publish MCA and TDC as NDArrays for plugin chain
        if (running)
        {
            int arrayCallbacks;
            getIntegerParam(NDArrayCallbacks, &arrayCallbacks);

            if (arrayCallbacks)
            {
                // --- MCA NDArray (SPECTRUM_SIZE × numElements, Int32) on addr 0 ---
                size_t mcaDims[2] = { static_cast<size_t>(SPECTRUM_SIZE),
                                      static_cast<size_t>(numElements) };
                NDArray *pMCA = this->pNDArrayPool->alloc(2, mcaDims, NDInt32, 0, nullptr);
                if (pMCA)
                {
                    epicsInt32 *pDest = static_cast<epicsInt32*>(pMCA->pData);
                    size_t total = static_cast<size_t>(numElements) * SPECTRUM_SIZE;
                    for (size_t i = 0; i < total; i++)
                        pDest[i] = mcaData[i].load(std::memory_order_relaxed);

                    pMCA->uniqueId = arrayCounter;
                    updateTimeStamp(&pMCA->epicsTS);
                    pMCA->timeStamp = pMCA->epicsTS.secPastEpoch
                                    + pMCA->epicsTS.nsec * 1e-9;
                    pMCA->pAttributeList->add("ColorMode", "Color mode",
                        NDAttrInt32, &colorMode);

                    this->unlock();
                    doCallbacksGenericPointer(pMCA, NDArrayData, 0);
                    this->lock();
                    pMCA->release();
                }

                // --- TDC NDArray (TDC_SIZE × numElements, Int32) on addr 1 ---
                size_t tdcDims[2] = { static_cast<size_t>(TDC_SIZE),
                                      static_cast<size_t>(numElements) };
                NDArray *pTDC = this->pNDArrayPool->alloc(2, tdcDims, NDInt32, 0, nullptr);
                if (pTDC)
                {
                    epicsInt32 *pDest = static_cast<epicsInt32*>(pTDC->pData);
                    size_t total = static_cast<size_t>(numElements) * TDC_SIZE;
                    for (size_t i = 0; i < total; i++)
                        pDest[i] = tdcData[i].load(std::memory_order_relaxed);

                    pTDC->uniqueId = arrayCounter;
                    updateTimeStamp(&pTDC->epicsTS);
                    pTDC->timeStamp = pTDC->epicsTS.secPastEpoch
                                    + pTDC->epicsTS.nsec * 1e-9;
                    pTDC->pAttributeList->add("ColorMode", "Color mode",
                        NDAttrInt32, &colorMode);

                    this->unlock();
                    doCallbacksGenericPointer(pTDC, NDArrayData, 1);
                    this->lock();
                    pTDC->release();
                }

                arrayCounter++;
                setIntegerParam(NDArrayCounter, arrayCounter);
            }
        }

        callParamCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: Data processing thread stopped\n"
             , portName
             );
}

//===========================================================================//

void GermaniumDetector::dataWriteThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->dataWriteThread();
}

void GermaniumDetector::dataWriteThread()
{
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: Data write thread started\n"
             , portName
             );

    while ( threadsRunning.load() )
    {
        epicsEventWaitWithTimeout(dataWriteAvailable, 1.0);
        flushWriteBuffer();
    }

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: Data write thread stopped\n"
             , portName
             );
}

//===========================================================================//

void GermaniumDetector::startDataAcquisition()
{
    if (acquisitionRunning.load()) return;

    clearSpectra();
    currentSegmentNumber.store(0);;
    totalBytesWritten.store(0);
    totalFilesWritten.store(0);

    createDataDirectory();
    //fileWritingEnabled = true;
    setAcquisitionRunning(true);

    // Start hardware acquisition via ZMQ register write
    zmqTx(ZMQ_CMD_REG_WRITE, TRIG, 1);

    setIntegerParam(GermaniumCNT, 1);
    callParamCallbacks();
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: acquisition started\n"
             , portName
             );
}

//===========================================================================//

void GermaniumDetector::stopDataAcquisition()
{
    if (!acquisitionRunning.load()) return;

    zmqTx(ZMQ_CMD_REG_WRITE, TRIG, 0);
    setAcquisitionRunning(false);
    //fileWritingEnabled = false;

    flushWriteBuffer();
    closeCurrentDataFile();

    setIntegerParam(GermaniumCNT, 0);
    callParamCallbacks();
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: acquisition stopped. %zu bytes in %d files\n"
             , portName
             , totalBytesWritten.load()
             , totalFilesWritten.load()
             );
}

//===========================================================================//

void GermaniumDetector::createDataDirectory()
{
    char dirPath[256];
    getStringParam(GermaniumDIR, sizeof(dirPath), dirPath);

    struct stat st = {};
    if (stat(dirPath, &st) == -1)
    {
        if (mkdir(dirPath, 0755) == 0)
            asynPrint( pasynUserSelf
                     , ASYN_TRACE_FLOW
                     , "%s: created directory %s\n"
                     , portName
                     , dirPath
                     );
        else
            asynPrint( pasynUserSelf
                     , ASYN_TRACE_ERROR
                     , "%s: failed to create directory %s: %s\n"
                     , portName
                     , dirPath
                     , strerror(errno)
                     );
    }
}

//===========================================================================//

std::string GermaniumDetector::generateFilename(int segmentNumber)
{
    char dir[256], fname[256];
    int runno;
    getStringParam(GermaniumDIR, sizeof(dir), dir);
    getStringParam(GermaniumFNAM, sizeof(fname), fname);
    getIntegerParam(GermaniumRUNNO, &runno);

    std::string path(dir);
    if (!path.empty() && path.back() != '/') path += '/';

    char buf[512];
    snprintf(buf, sizeof(buf), "%s%s-%06d-%03d.bin",
             path.c_str(), fname, runno, segmentNumber);
    return std::string(buf);
}

//===========================================================================//

bool GermaniumDetector::openNewDataFile()
{
    closeCurrentDataFile();
    std::string filename = generateFilename(currentSegmentNumber.load());

    auto fileHandle = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fileHandle < 0)
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "%s: failed to open %s: %s\n"
                 , portName
                 , filename.c_str()
                 , strerror(errno)
                 );
        return false;
    }
    currentFileHandle.store(fileHandle);
    currentFileSize.store(0);
    currentFilename = filename;
    return true;
}

//===========================================================================//

void GermaniumDetector::closeCurrentDataFile()
{
    if (currentFileHandle.load() >= 0)
    {
        close(currentFileHandle.load());
        currentFileHandle.store(-1);
        totalFilesWritten.fetch_add(1);
        totalBytesWritten.fetch_add(currentFileSize.load());
        currentFileSize.store(0);
    }
}

//===========================================================================//

bool GermaniumDetector::writeDataToFile(const uint8_t* data, size_t dataSize)
{
    if (!udpDataFileWriteEnable.load() || !data || dataSize == 0) return false;

    int maxSizeMB;
    getIntegerParam(GermaniumFSIZE, &maxSizeMB);
    size_t maxSize = static_cast<size_t>(maxSizeMB) * 1024 * 1024;

    auto fileHandle = currentFileHandle.load();
    if (fileHandle < 0 || (currentFileSize.load() + dataSize) > maxSize)
    {
        if (fileHandle >= 0)
        {
            closeCurrentDataFile();
            currentSegmentNumber.fetch_add(1);
        }
        if (!openNewDataFile()) return false;
    }

    ssize_t written = write(fileHandle, data, dataSize);
    if (written != static_cast<ssize_t>(dataSize)) return false;

    currentFileSize.fetch_add(dataSize, std::memory_order_relaxed);
    return true;
}

//===========================================================================//

/*
 * Enqueue raw data into the lock-free MPSC block queue.
 *
 * Producers (zmqDataThread / plUdpDataThread) claim a slot via CAS on
 * dataQueueHead, memcpy the payload, then publish with release-store on
 * the per-block state flag.  No mutex is touched on the hot path.
 */
void GermaniumDetector::addDataToWriteBuffer(const uint8_t* data, size_t dataSize)
{
    if (!data || dataSize == 0 || dataSize > DATA_BLOCK_SIZE) return;

    // CAS loop to claim the next slot
    uint64_t head = dataQueueHead.load(std::memory_order_relaxed);
    for (;;)
    {
        uint64_t tail = dataQueueTail.load(std::memory_order_acquire);
        if (head - tail >= static_cast<uint64_t>(DATA_QUEUE_CAPACITY))
            return;  // queue full — drop this chunk

        if (dataQueueHead.compare_exchange_weak( head, head + 1
                                               , std::memory_order_acq_rel
                                               , std::memory_order_relaxed))
            break;
    }

    // We now own slot `head`
    DataBlock &block = dataQueue[head & DATA_QUEUE_MASK];
    memcpy(block.data, data, dataSize);
    block.size = static_cast<uint32_t>(dataSize);
    block.state.store(DATA_BLOCK_READY, std::memory_order_release);

    epicsEventSignal(dataWriteAvailable);
}

//===========================================================================//

/*
 * Drain the lock-free queue from the consumer side (dataWriteThread).
 *
 * Reads contiguously from dataQueueTail while blocks are in READY state.
 * A block stuck in CLAIMED means a producer hasn't finished its memcpy
 * yet — we stop and retry on the next wakeup to preserve ordering.
 */
void GermaniumDetector::flushWriteBuffer()
{
    uint64_t tail = dataQueueTail.load(std::memory_order_relaxed);
    uint64_t head = dataQueueHead.load(std::memory_order_acquire);

    while (tail < head)
    {
        DataBlock &block = dataQueue[tail & DATA_QUEUE_MASK];

        if (block.state.load(std::memory_order_acquire) != DATA_BLOCK_READY)
            break;  // producer still writing — preserve ordering

        if ( udpDataFileWriteEnable.load() && block.data && block.size )
        {
            writeDataToFile(block.data, block.size);
        }
        block.state.store(DATA_BLOCK_FREE, std::memory_order_release);
        ++tail;
    }

    dataQueueTail.store(tail, std::memory_order_release);
}

//===========================================================================//
