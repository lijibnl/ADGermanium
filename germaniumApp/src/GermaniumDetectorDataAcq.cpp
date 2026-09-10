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

#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <print>
#include <cstdio>
#include <chrono>
#include <thread>
#include <iostream>

#include "GermaniumDetector.hpp"
#include "LockFreeBroadcastSPMC.hpp"


//===========================================================================//

// PL UDP data markers
#define SOF_MARKER  0xFEEDFACE
#define EOF_MARKER  0xDECAFBAD

//===========================================================================//

using Clock = std::chrono::steady_clock;

//===========================================================================//

namespace {
    constexpr float DATA_THREAD_WAIT_FOR_DATA_TIMEOUT = 0.02f; // 10 - 50 ms

    constexpr size_t DATA_PROC_THREAD_INDEX  = 0;
    constexpr size_t DATA_WRITE_THREAD_INDEX = 1;


    constexpr std::chrono::milliseconds DATA_THREAD_WAIT_FOR_EOF_TIMEOUT = std::chrono::milliseconds(500);  // 500 ms
    //constexpr std::chrono::milliseconds DATA_THREAD_IDLE_TIMEOUT = std::chrono::milliseconds(500);  // 500 ms
    
    enum class QueueConsumerThreadState
    {
        IDLE,
        START,
        RUNNING,
        FLUSH,
        FINISH
    };
}

//===========================================================================//

bool GermaniumDetector::udpInit()
{
    allocateDataArrays();

    if (!initializePlUdpSocket())
    {
        std::cerr << "[" << __func__ << "]: failed to initialize PL UDP socket\n";
        return false;
    }
    
    //------------------------------------------------------------------//

    spectraSynchronizeThreadId = epicsThreadCreate( "GermaniumDataSync"
                                                  , epicsThreadPriorityMedium
                                                  , epicsThreadGetStackSize(epicsThreadStackMedium)
                                                  , spectraSynchronizeThreadC
                                                  , this
                                                  );
    if (!spectraSynchronizeThreadId)
    {
        std::cerr << "[" << __func__ << "]: failed to create data synchronizing thread\n";
        return false;
    }
    std::cerr << "[" << __func__ << "]: UDP data synchronizing thread started\n";
    
    //------------------------------------------------------------------//

    dataProcessThreadId = epicsThreadCreate( "GermaniumDataProc"
                                           , epicsThreadPriorityMedium
                                           , epicsThreadGetStackSize(epicsThreadStackMedium)
                                           , dataProcessThreadC
                                           , this
                                           );
    if (!dataProcessThreadId)
    {
        std::cerr << "[" << __func__ << "]: failed to create data processing thread\n";
        return false;
    }
    std::cerr << "[" << __func__ << "]: UDP data processing thread started\n";
    
    //------------------------------------------------------------------//

    dataWriteThreadId = epicsThreadCreate( "GermaniumDataWrite"
                                         , epicsThreadPriorityMedium
                                         , epicsThreadGetStackSize(epicsThreadStackMedium)
                                         , dataWriteThreadC
                                         , this
                                         );
    if (!dataWriteThreadId)
    {
        std::cerr << "[" << __func__ << "]: failed to create UDP data write thread\n";
        return false;
    }
    std::cerr << "[" << __func__ << "]: UDP data write thread started\n";
    
    //------------------------------------------------------------------//

    plUdpDataThreadId = epicsThreadCreate( "GermaniumPlUdp"
                                         , epicsThreadPriorityHigh
                                         , epicsThreadGetStackSize(epicsThreadStackMedium)
                                         , plUdpDataThreadC
                                         , this
                                         );
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: PL UDP data thread started\n"
             , __func__
             );
    if (!plUdpDataThreadId)
    {
        std::cerr << "[" << __func__ << "]: failed to create PL UDP data thread\n";
        return false;
    }
    std::cerr << "[" << __func__ << "]: PL UDP data thread started\n";

    //------------------------------------------------------------------//

    udpWatchdogThreadId = epicsThreadCreate( "GermaniumUdpWatch"
                                           , epicsThreadPriorityMedium
                                           , epicsThreadGetStackSize(epicsThreadStackMedium)
                                           , udpWatchdogThreadC
                                           , this
                                           );
    if (!udpWatchdogThreadId)
    {
        std::cerr << "[" << __func__ << "]: failed to create UDP watchdog thread\n";
        return false;
    }
    std::cerr << "[" << __func__ << "]: UDP watchdog thread started\n";

    //------------------------------------------------------------------//

    requestUdpReinitialization();

    return true;
}

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
                 , __func__
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
                 , __func__
                 , PL_UDP_DATA_PORT
                 , strerror(errno)
                 );
        close(plUdpSocket);
        plUdpSocket = -1;
        return false;
    }

    plUdpInitialized = true;
    std::cerr << __func__
              << ": PL UDP data socket bound to port "
              << PL_UDP_DATA_PORT
              << "\n";
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
    // Allocate lock-free block queue
    dataQueue = std::make_unique<LockFreeBroadcastSPMC<DataBlock, DATA_QUEUE_CAPACITY, 2>>();

    // Flat atomic arrays — safe for concurrent access from multiple
    // producer threads (zmqData, plUdp) and the EPICS read thread.
    size_t mcaTotal = static_cast<size_t>(numElements) * SPECTRUM_SIZE;
    size_t tdcTotal = static_cast<size_t>(numElements) * TDC_SIZE;

    mcaData    = std::make_unique<std::atomic<uint32_t>[]>(mcaTotal);
    tdcData    = std::make_unique<std::atomic<uint32_t>[]>(tdcTotal);
    countRates = std::make_unique<std::atomic<uint32_t>[]>(numElements);
    totalCounts= std::make_unique<std::atomic<uint64_t>[]>(numElements);

    clearSpectra();

    std::cerr << __func__
              << ": allocated data arrays for "
              << numElements
              << " elements\n"
              ;
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

        DataBlock* block = nullptr;

        for (int spins = 0; ; spins++)
        {
            block = dataQueue->pushRequest();
            if (block) break;

            if (spins < 10)
            {
                std::this_thread::yield();
            }
            else
            {
                epicsThreadSleepQuantum();
            }
        }

        ssize_t bytesReceived = recvfrom( plUdpSocket
                                        , block->data
                                        , DATA_BLOCK_SIZE
                                        , 0
                                        , (struct sockaddr*)&senderAddr
                                        , &addrLen
                                        );
        if (bytesReceived <= 0) 
        {
            dataQueue->pushCancelRequest();
            continue;
        }

        //std::println("[{}]: {} bytes received", __func__, bytesReceived);
        
        block->size = static_cast<size_t>(bytesReceived);
        dataQueue->push();

        // Notify consumers for available data
        for ( auto& evt : udpDataAvailableEvent)
            evt.trigger();
    }

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: PL UDP data thread stopped\n"
             , __func__
             );
}

//===========================================================================//

void GermaniumDetector::dataProcessThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->dataProcessThread();
}

void GermaniumDetector::dataProcessThread()
{
    QueueConsumerThreadState threadState = QueueConsumerThreadState::IDLE;
    Clock::time_point lastPacketTime;

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: Data processing thread started\n"
             , __func__
             );

    while ( threadsRunning.load() )
    {
        switch( threadState )
        {
            //----------------------------------------------------//
            case QueueConsumerThreadState::IDLE:
            {
                //std::println("[{}]: in IDLE state", __func__);
                for (int spins = 0; spins < 100; spins++)
                {
                    if (acquisitionRunning.load())
                    {
                        threadState = QueueConsumerThreadState::START;
                        break;
                    }

                    if (spins < 10)
                    {
                        std::this_thread::yield();
                    }
                    else
                    {
                        epicsThreadSleepQuantum();
                    }
                }
                break;
            }
            //----------------------------------------------------//
            case QueueConsumerThreadState::START:
            {
                //std::println("[{}]: in START state", __func__);
                udpDataAvailableEvent[DATA_PROC_THREAD_INDEX].wait(
                                        DATA_THREAD_WAIT_FOR_DATA_TIMEOUT
                                        );
                auto dataBlock = dataQueue->popRequest(DATA_PROC_THREAD_INDEX);
                if ( dataBlock )
                {
                    if ( dataBlock->size > 4 )
                    {
                        // Parse packet as big-endian 32-bit words
                        size_t numWords = dataBlock->size / sizeof(uint32_t) - 4;
                        uint32_t *words = reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(dataBlock->data)) + 4;

                        calcSpectra( words, numWords );

                        //// Process event data words (skip headers, detect SOF/EOF markers)
                        //for (size_t i = 4; i + 1 < numWords; i += 2)
                        //{
                        //    uint32_t w1 = ntohl(words[i]);
                        //    uint32_t w2 = ntohl(words[i + 1]);

                        //    calcSpectra(w1, w2);
                        //}
                        threadState = QueueConsumerThreadState::RUNNING;
                    }
                    dataQueue->pop(DATA_PROC_THREAD_INDEX);
                }
                break;
            }
            //----------------------------------------------------//
            case QueueConsumerThreadState::RUNNING:
            {
                //std::println("[{}]: in RUNNING state", __func__);
                bool queueNotEmpty = true;
                udpDataAvailableEvent[DATA_PROC_THREAD_INDEX].wait(
                                        DATA_THREAD_WAIT_FOR_DATA_TIMEOUT
                                        );

                // Loop to drain all available data blocks in the queue
                while ( threadState == QueueConsumerThreadState::RUNNING )
                {
                    auto dataBlock = dataQueue->popRequest(DATA_PROC_THREAD_INDEX);
                    if ( dataBlock  )
                    {
                        if ( dataBlock->size > 4 )
                        {
                            //std::println("[{}]: got data. Calculating spectra...", __func__);
                            // Parse packet as big-endian 32-bit words
                            size_t numWords = dataBlock->size / sizeof(uint32_t) - 2;
                            uint32_t *words = reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(dataBlock->data)) + 2;

                            calcSpectra( words, numWords );
                            //// Process event data words (skip headers, detect SOF/EOF markers)
                            //for (size_t i = 2; i + 1 < numWords; i += 2)
                            //{
                            //    uint32_t w1 = ntohl(words[i]);
                            //    uint32_t w2 = ntohl(words[i + 1]);
                            //    calcSpectra(w1, w2);
                            //}
                        }

                        // Pop the processed data block from the queue
                        queueNotEmpty = dataQueue->pop(DATA_PROC_THREAD_INDEX);
                                        
                        if ( !acquisitionRunning.load() )
                        {
                            threadState = QueueConsumerThreadState::FLUSH;
                            lastPacketTime = Clock::now();
                        }
                        
                        if ( !queueNotEmpty)
                        {
                            break; // no more data in the queue
                        }
                    }
                    else
                    {
                        if (!acquisitionRunning.load())
                        {
                            threadState = QueueConsumerThreadState::FLUSH;
                            lastPacketTime = Clock::now();
                        }
                        break;
                    }
               }
                break;
            }
            //----------------------------------------------------//
            case QueueConsumerThreadState::FLUSH:
            {
                //std::println("[{}]: in FLUSH state", __func__);
                while ( threadState == QueueConsumerThreadState::FLUSH )
                {
                    udpDataAvailableEvent[DATA_PROC_THREAD_INDEX].wait(
                                            DATA_THREAD_WAIT_FOR_DATA_TIMEOUT
                                            );

                    auto dataBlock = dataQueue->popRequest(DATA_PROC_THREAD_INDEX);
                    if ( dataBlock )
                    {
                        if ( dataBlock->size > 4)
                        {
                            // Parse packet as big-endian 32-bit words
                            size_t numWords = dataBlock->size / sizeof(uint32_t) - 2;
                            uint32_t *words = reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(dataBlock->data)) + 2;

                            if (words[numWords-1] == EOF_MARKER)
                            {
                                numWords -=2;
                                threadState = QueueConsumerThreadState::FINISH;
                            }

                            calcSpectra( words, numWords );

                            lastPacketTime = Clock::now();
                        }

                        // Pop the processed data block from the queue
                        if (!dataQueue->pop(DATA_PROC_THREAD_INDEX))
                        {
                            break; // no more data in the queue
                        }
                    }

                    if ( (Clock::now() - lastPacketTime ) > DATA_THREAD_WAIT_FOR_EOF_TIMEOUT )
                    {
                        threadState = QueueConsumerThreadState::FINISH;
                    }
                }

                break;
            }
            //----------------------------------------------------//
            case QueueConsumerThreadState::FINISH:
            {
                std::println("[{}]: finishing data processing.", __func__);
                threadState = QueueConsumerThreadState::IDLE;

                publishSpectra();

                //uint64_t totalMca = 0;
                //size_t nonzeroBins = 0;
                //size_t firstNonzero = 0;
                //uint32_t firstValue = 0;

                //const size_t totalBins = static_cast<size_t>(numElements) * SPECTRUM_SIZE;
                //for (size_t i = 0; i < totalBins; i++)
                //{
                //    uint32_t bin = mcaData[i].load(std::memory_order_relaxed);
                //    totalMca += bin;
                //    if (bin != 0)
                //    {
                //        if (nonzeroBins == 0)
                //        {
                //            firstNonzero = i;
                //            firstValue = bin;
                //        }
                //        nonzeroBins++;
                //        std::println(" mcaData[{}] = {}", i, bin);
                //    }
                //}

                //std::println("[{}]: MCA total={}, nonzeroBins={}, firstNonzero={}, firstValue={}",
                //            __func__, totalMca, nonzeroBins, firstNonzero, firstValue);
                break;
            }
            //----------------------------------------------------//
            default:
            {
                asynPrint( pasynUserSelf
                         , ASYN_TRACE_ERROR
                         , "%s: Unknown consumer thread state\n"
                         , __func__
                         );
                threadState = QueueConsumerThreadState::IDLE;
                break;
            }
            //----------------------------------------------------//
        }
    }

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: data processing thread stopped\n"
             , __func__
             );
}

//===========================================================================//

void GermaniumDetector::calcSpectra( uint32_t* words, size_t numWords )
{
    static uint32_t numEvents = 0;
    static uint32_t numValidEvents = 0;

    for (size_t i = 0; i + 1 < numWords; i += 2)
    {
        uint32_t w1 = ntohl(words[i]);

        // w2 might be used here but is to be implemented
        //uint32_t w2 = ntohl(words[i + 1]);

        int chip = (w1 >> 27) & 0xF;
        int chan  = (w1 >> 22) & 0x1F;
        int td   = (w1 >> 12) & 0x3FF;
        int pd   = w1 & 0xFFF;

        int element = chip * 32 + chan;
        if (element >= 0 && element < numElements)
        {
            mcaData[element * SPECTRUM_SIZE + pd].fetch_add(1, std::memory_order_relaxed);
            tdcData[element * TDC_SIZE + td].fetch_add(1, std::memory_order_relaxed);
            countRates[element].fetch_add(1, std::memory_order_relaxed);
            totalCounts[element].fetch_add(1, std::memory_order_relaxed);
            evttot.fetch_add(1, std::memory_order_relaxed);
            numEvents++;
            if (pd > 0) // example condition for a valid event
            {
                numValidEvents++;
            }
        }
    }
    //std::println("[{}]: numEvents = {}, numValidEvents = {}", __func__, numEvents, numValidEvents);
}

void GermaniumDetector::publishSpectra()
{
    this->lock();

    int monch, chip;
    getIntegerParam( GermaniumMONCH, &monch );
    getIntegerParam( GermaniumCHIP, &chip );

    int element = chip * 32 + monch;
    if (element <0 ) element = 0;
    if (element >= numElements) element = numElements-1;

    
    const size_t mcaTotal = static_cast<size_t>(numElements) * SPECTRUM_SIZE;
    const size_t tdcTotal = static_cast<size_t>(numElements) * TDC_SIZE;
    const size_t spectrumTotal = SPECTRUM_SIZE;
    const size_t spectrumOffset = static_cast<size_t>(element) * SPECTRUM_SIZE;
    const size_t intensityTotal = static_cast<size_t>(numElements);

    std::vector<epicsInt32> mcaBuffer(mcaTotal);
    std::vector<epicsInt32> tdcBuffer(tdcTotal);
    std::vector<epicsInt32> spectrumBuffer(spectrumTotal);
    std::vector<epicsFloat64> spectrumXBuffer(spectrumTotal);
    std::vector<epicsInt32> intensityBuffer(intensityTotal);

    for (size_t i = 0; i < mcaTotal; i++)
        mcaBuffer[i] = static_cast<epicsInt32>(mcaData[i].load(std::memory_order_relaxed));

    for (size_t i = 0; i < tdcTotal; i++)
        tdcBuffer[i] = static_cast<epicsInt32>(tdcData[i].load(std::memory_order_relaxed));

    for (size_t i = 0; i < spectrumTotal; i++)
        spectrumBuffer[i] = static_cast<epicsInt32>(mcaData[spectrumOffset + i].load(std::memory_order_relaxed));

    for (size_t i = 0; i < spectrumTotal; i++)
        spectrumXBuffer[i] = static_cast<epicsFloat64>(i) * 0.1;

    for (size_t i = 0; i < intensityTotal; i++)
        intensityBuffer[i] = static_cast<epicsInt32>(countRates[i].load(std::memory_order_relaxed));

    doCallbacksInt32Array(mcaBuffer.data(), mcaTotal, GermaniumMCA, 0);
    doCallbacksInt32Array(tdcBuffer.data(), tdcTotal, GermaniumTDC, 0);
    doCallbacksInt32Array(spectrumBuffer.data(), spectrumTotal, GermaniumSPCT, 0);
    doCallbacksFloat64Array(spectrumXBuffer.data(), spectrumTotal, GermaniumSPCTX, 0);
    doCallbacksInt32Array(intensityBuffer.data(), intensityTotal, GermaniumINTENS, 0);

    int arrayCallbacks = 0;
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    if (!arrayCallbacks)
    {
        callParamCallbacks();
        this->unlock();
        return;
    }

    int colorMode = NDColorModeMono;

    size_t mcaDims[2] = {
        static_cast<size_t>(SPECTRUM_SIZE),
        static_cast<size_t>(numElements)
    };

    NDArray *pMCA = this->pNDArrayPool->alloc(2, mcaDims, NDInt32, 0, nullptr);
    if (pMCA)
    {
        auto *pDest = static_cast<epicsInt32*>(pMCA->pData);
        std::copy(mcaBuffer.begin(), mcaBuffer.end(), pDest);

        pMCA->uniqueId = arrayCounter;
        updateTimeStamp(&pMCA->epicsTS);
        pMCA->timeStamp = pMCA->epicsTS.secPastEpoch
                         + pMCA->epicsTS.nsec * 1e-9;
        pMCA->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);

        this->unlock();
        doCallbacksGenericPointer(pMCA, NDArrayData, 0);
        this->lock();

        pMCA->release();
    }

    size_t tdcDims[2] = {
        static_cast<size_t>(TDC_SIZE),
        static_cast<size_t>(numElements)
    };

    NDArray *pTDC = this->pNDArrayPool->alloc(2, tdcDims, NDInt32, 0, nullptr);
    if (pTDC)
    {
        auto *pDest = static_cast<epicsInt32*>(pTDC->pData);
        std::copy(tdcBuffer.begin(), tdcBuffer.end(), pDest);

        pTDC->uniqueId = arrayCounter;
        updateTimeStamp(&pTDC->epicsTS);
        pTDC->timeStamp = pTDC->epicsTS.secPastEpoch
                         + pTDC->epicsTS.nsec * 1e-9;
        pTDC->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);

        this->unlock();
        doCallbacksGenericPointer(pTDC, NDArrayData, 1);
        this->lock();

        pTDC->release();
    }

    arrayCounter++;
    setIntegerParam(NDArrayCounter, arrayCounter);
    callParamCallbacks();

    this->unlock();
}

//===========================================================================//



void GermaniumDetector::spectraSynchronizeThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->spectraSynchronizeThread();
}

void GermaniumDetector::spectraSynchronizeThread()
{
    while (threadsRunning.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        setIntegerParam(GermaniumSS, acquisitionRunning.load() ? 1 : 0);
        publishSpectra();
    }
}
/*
{
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: Spectra synchronizing thread started\n"
             , __func__
             );

    int arrayCounter = 0;
    int colorMode = NDColorModeMono;

    while ( threadsRunning.load() )
    {
        // Synchronize every 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));

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
             , __func__
             );
}
*/

//===========================================================================//

void GermaniumDetector::dataWriteThreadC(void *pPvt)
{
    static_cast<GermaniumDetector*>(pPvt)->dataWriteThread();
}

void GermaniumDetector::dataWriteThread()
{
    QueueConsumerThreadState threadState = QueueConsumerThreadState::IDLE;
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: Data write thread started\n"
             , __func__
             );

    Clock::time_point lastPacketTime = Clock::now();
    
    while ( threadsRunning.load() )
    {
        
        switch(threadState)
        {
            //----------------------------------------------------//
            case QueueConsumerThreadState::IDLE:
            {
                //std::println("[{}]: in IDLE state", __func__);
                for (int spins = 0; spins < 100; spins++)
                {
                    if (acquisitionRunning.load())
                    {
                        threadState = QueueConsumerThreadState::RUNNING;
                        lastPacketTime = Clock::now();
                        break;
                    }

                    if (spins < 10)
                    {
                        std::this_thread::yield();
                    }
                    else
                    {
                        epicsThreadSleepQuantum();
                    }
                }
                break;
            }
            //----------------------------------------------------//
            case QueueConsumerThreadState::RUNNING:
            {
                //std::println("[{}]: in RUNNING state", __func__);
                udpDataAvailableEvent[DATA_WRITE_THREAD_INDEX].wait(
                                        DATA_THREAD_WAIT_FOR_DATA_TIMEOUT
                                        );

                while ( threadState == QueueConsumerThreadState::RUNNING )
                {
                    while ( auto dataBlock = dataQueue->popRequest(DATA_WRITE_THREAD_INDEX) )
                    {
                        if (dataBlock->size < 8)
                        {
                            dataQueue->pop(DATA_WRITE_THREAD_INDEX);
                            continue;
                        }
                        
                        //std::println("[{}]: got data", __func__);

                        if ( !writeDataToFile( dataBlock ) )
                        {
                            std::cerr << __func__
                                      << ": failed to write data to file\n";
                        }
                        
                        if ( !acquisitionRunning.load() )
                        {
                            threadState = QueueConsumerThreadState::FLUSH;
                        }

                        // Pop the processed data block from the queue.
                        // Continue with the next if more data in the queue.
                        if ( !dataQueue->pop(DATA_WRITE_THREAD_INDEX) )
                        {
                            asynPrint( pasynUserSelf
                                    , ASYN_TRACE_FLOW
                                    , "%s: no more data in the queue\n"
                                    , __func__
                                    );
                        }
                    }
                    if (!acquisitionRunning.load())
                    {
                        threadState = QueueConsumerThreadState::FLUSH;
                        lastPacketTime = Clock::now();
                    }
                    else
                    {
                        break;
                    }
                }
                break;
            }
            //----------------------------------------------------//
            case QueueConsumerThreadState::FLUSH:
            {
                //std::println("[{}]: in FLUSH state", __func__);
                udpDataAvailableEvent[DATA_WRITE_THREAD_INDEX].wait(
                                        DATA_THREAD_WAIT_FOR_DATA_TIMEOUT
                                        );

                while ( threadState == QueueConsumerThreadState::FLUSH )
                {
                    while ( auto dataBlock = dataQueue->popRequest(DATA_WRITE_THREAD_INDEX) )
                    {
                        if (dataBlock->size < 8)
                        {
                            dataQueue->pop(DATA_WRITE_THREAD_INDEX);
                            continue;
                        }

                        if ( !writeDataToFile( dataBlock ) )
                        {
                            std::cerr << __func__
                                      << ": failed to write data to file\n";
                        }
                        const size_t numWords = dataBlock->size / sizeof(uint32_t);
                        const uint32_t *words = reinterpret_cast<const uint32_t*>(dataBlock->data);

                        uint32_t w = ntohl(words[numWords - 1]);
                        if ( w == EOF_MARKER )
                        {
                            threadState = QueueConsumerThreadState::IDLE;
                            closeCurrentDataFile();
                        }

                        // Pop the processed data block from the queue.
                        // Continue with the next if more data in the queue.
                        if ( !dataQueue->pop(DATA_WRITE_THREAD_INDEX) )
                        {
                            asynPrint( pasynUserSelf
                                    , ASYN_TRACE_FLOW
                                    , "%s: no more data in the queue\n"
                                    , __func__
                                    );
                        }
                        lastPacketTime = Clock::now();
                    }
                    auto now = Clock::now();
                    if ( (now - lastPacketTime ) > DATA_THREAD_WAIT_FOR_EOF_TIMEOUT)
                    {
                        threadState = QueueConsumerThreadState::IDLE;
                        closeCurrentDataFile();
                    }
                }
                break;
            }
            //----------------------------------------------------//
            default:
            {
                asynPrint( pasynUserSelf
                         , ASYN_TRACE_ERROR
                         , "%s: Unknown consumer thread state\n"
                         , __func__
                         );
                threadState = QueueConsumerThreadState::IDLE;
                break;
            }
            //----------------------------------------------------//
        }
    }

    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: Data write thread stopped\n"
             , __func__
             );
}

//===========================================================================//

void GermaniumDetector::startDataAcquisition()
{
    if (acquisitionRunning.load()) return;

    clearSpectra();
    currentSegmentNumber.store(0);
    totalBytesWritten.store(0);
    totalFilesWritten.store(0);

    createDataDirectory();
    closeCurrentDataFile();  // In case a previous data file is still open due to loss of last packet
    setAcquisitionRunning(true);

    // Start hardware acquisition via ZMQ register write
    zmqTx(GermaniumProtocol::Command::REG_WRITE, GermaniumProtocol::Register::TRIG, 1);

    setIntegerParam(GermaniumCNT, 1);
    callParamCallbacks();
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: acquisition started\n"
             , __func__
             );
}

//===========================================================================//

void GermaniumDetector::stopDataAcquisition()
{
    if (!acquisitionRunning.load()) return;

    zmqTx(GermaniumProtocol::Command::REG_WRITE, GermaniumProtocol::Register::TRIG, 0);
    setAcquisitionRunning(false);

    //flushWriteBuffer();

    setIntegerParam(GermaniumCNT, 0);
    callParamCallbacks();
    asynPrint( pasynUserSelf
             , ASYN_TRACE_FLOW
             , "%s: acquisition stopped. %zu bytes in %d files\n"
             , __func__
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
                     , __func__
                     , dirPath
                     );
        else
            asynPrint( pasynUserSelf
                     , ASYN_TRACE_ERROR
                     , "%s: failed to create directory %s: %s\n"
                     , __func__
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
                 , __func__
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

bool GermaniumDetector::writeDataToFile( const DataBlock* block )
{
    size_t dataSize = block->size;
    if (!udpDataFileWriteEnable.load()) return false;

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

    ssize_t written = write(fileHandle, block->data, dataSize);

    /*
    // Last few data blocks in the queue, look for EOF
    if (flushWriteBuffer.load())
    {
        const size_t numWords = block->size / sizeof(uint32_t);
        const uint32_t *words = reinterpret_cast<const uint32_t*>(block->data);
        if (words[numWords-1] == EOF_MARKER)
        {
            closeCurrentDataFile();
        }
    }
    */

    if (written != static_cast<ssize_t>(dataSize))
    {
        asynPrint( pasynUserSelf
                 , ASYN_TRACE_ERROR
                 , "%s: failed to write data to file: %s\n"
                 , __func__
                 , strerror(errno)
                 );
        return false;
    }

    currentFileSize.fetch_add(dataSize, std::memory_order_relaxed);
    return true;
}

//===========================================================================//
