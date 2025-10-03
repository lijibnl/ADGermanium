/**
 * @file germaniumDetectorDataAcq.cpp
 * @brief Handles UDP data reception, buffering, and multi-segment file
 *        writing.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/11/2025
 * @copyright
 * Copyright (c) 2025 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include "germaniumDetector.hpp"
#include "germaniumDetectorTypes.hpp"
#include "germaniumDetectorBuff.hpp"
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <chrono>
#include <span>

//===========================================================================//

/*
 * Data acquisition and file management initialization
 */
void germaniumDetector::setupDataAcquisition()
{
    printf("Setting up data acquisition system...\n");
    
    // Initialize acquisition parameters
    acquisitionRunning = false;
    evttot = 0;
    framestat = 0;
    file_size = 0;
    seg_num = 0;
    file_handle = -1;
    
    // Initialize file writing state
    fileWritingEnabled = false;
    totalBytesWritten = 0;
    totalFilesWritten = 0;
    
    // Create directory if it doesn't exist
    createDataDirectory();
    
    // Clear all spectrum data
    for (size_t i = 0; i < mcaData.size(); i++)
    {
        std::fill(mcaData[i].begin(), mcaData[i].end(), 0);
        std::fill(tdcData[i].begin(), tdcData[i].end(), 0);
    }
    
    std::fill(countRates.begin(), countRates.end(), 0);
    std::fill(totalCounts.begin(), totalCounts.end(), 0);
    
    // Initialize data buffer for UDP reception
    if (!udpDataBuffer)
    {
        udpDataBuffer = std::make_unique<uint8_t[]>(UDP_BUFFER_SIZE);
    }
    
    // Create circular buffer for data writing thread
    dataWriteBuffer.resize(DATA_WRITE_BUFFER_SIZE);
    writeBufferHead = 0;
    writeBufferTail = 0;
    writeBufferCount = 0;
    
    printf("Data acquisition system initialized\n");
}

//===========================================================================//

/*
 * Create data directory if it doesn't exist
 */
void germaniumDetector::createDataDirectory()
{
    char dirPath[256];
    getStringParam(GermaniumDIR, sizeof(dirPath), dirPath);
    
    struct stat st = {0};
    if (stat(dirPath, &st) == -1)
    {
        if (mkdir(dirPath, 0755) == 0)
        {
            printf("Created data directory: %s\n", dirPath);
        }
        else
        {
            printf("Failed to create data directory: %s (error: %s)\n", 
                   dirPath, strerror(errno));
        }
    }
    else
    {
        printf("Data directory exists: %s\n", dirPath);
    }
}

//===========================================================================//

/*
 * Open new data file for writing
 */
bool germaniumDetector::openNewDataFile()
{
    // Close current file if open
    closeCurrentDataFile();
    
    // Generate new filename
    std::string filename = generateFilename(seg_num);
    
    // Open new file
    file_handle = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_handle < 0)
    {
        printf("Failed to open data file: %s (error: %s)\n", 
               filename.c_str(), strerror(errno));
        return false;
    }
    
    file_size = 0;
    file_name = filename;
    
    printf("Opened new data file: %s (segment %d)\n", 
           filename.c_str(), seg_num);
    
    return true;
}

//===========================================================================//

/*
 * Close current data file
 */
void germaniumDetector::closeCurrentDataFile()
{
    if (file_handle >= 0)
    {
        close(file_handle);
        printf("Closed data file: %s (size: %ld bytes)\n", 
               file_name.c_str(), file_size);
        
        file_handle = -1;
        totalFilesWritten++;
        totalBytesWritten += file_size;
        file_size = 0;
    }
}

//===========================================================================//

/*
 * Write data to current file, handling file size limits and segmentation
 */
bool germaniumDetector::writeDataToFile(const uint8_t* data, size_t dataSize)
{
    if (!fileWritingEnabled || !data || dataSize == 0)
    {
        return false;
    }
    
    int maxFileSize;
    getIntegerParam(GermaniumFSIZE, &maxFileSize);
    
    // Check if we need a new file
    if (file_handle < 0 || 
        (file_size + dataSize) > static_cast<size_t>(maxFileSize))
    {
        if (file_handle >= 0)
        {
            closeCurrentDataFile();
            seg_num++;
        }
        
        if (!openNewDataFile())
        {
            return false;
        }
    }
    
    // Write data to file
    ssize_t bytesWritten = write(file_handle, data, dataSize);
    if (bytesWritten != static_cast<ssize_t>(dataSize))
    {
        printf("Failed to write data to file: %s (error: %s)\n", 
               file_name.c_str(), strerror(errno));
        return false;
    }
    
    file_size += dataSize;
    
    // Sync file periodically for data safety
    if ((file_size % (1024*1024)) < dataSize) // Every ~1MB
    {
        fsync(file_handle);
    }
    
    return true;
}

//===========================================================================//

/*
 * Start data acquisition and file writing
 */
void germaniumDetector::startDataAcquisition()
{
    if (acquisitionRunning)
    {
        printf("Data acquisition already running\n");
        return;
    }
    
    // Reset counters and state
    evttot = 0;
    framestat = 0;
    seg_num = 0;
    totalBytesWritten = 0;
    totalFilesWritten = 0;
    
    // Create data directory
    createDataDirectory();
    
    // Enable file writing
    fileWritingEnabled = true;
    
    // Start acquisition
    acquisitionRunning = true;
    
    // Send start command to hardware
    udpRegisterWrite(TRIG, 1);
    
    printf("Data acquisition started\n");
}

//===========================================================================//

/*
 * Stop data acquisition and file writing
 */
void germaniumDetector::stopDataAcquisition()
{
    if (!acquisitionRunning)
    {
        printf("Data acquisition not running\n");
        return;
    }
    
    // Send stop command to hardware
    udpRegisterWrite(TRIG, 0);
    
    // Stop acquisition
    acquisitionRunning = false;
    fileWritingEnabled = false;
    
    // Close current data file
    
    // Flush any remaining data in write buffer
    flushWriteBuffer();
    
    printf("Data acquisition stopped. Total: %ld bytes in %d files\n", 
           totalBytesWritten, totalFilesWritten);
}

//===========================================================================//

/*
 * Data processing thread function (C wrapper)
 */
//extern "C"
void germaniumDetector::dataProcessingThreadC(void *drvPvt)
{
    germaniumDetector *pGermanium = static_cast<germaniumDetector*>(drvPvt);
    pGermanium->dataProcessingThread();
}

//===========================================================================//

/*
 * Data processing thread - handles spectrum updates.
 * This thread processes detector data and updates EPICS parameters
 */
void germaniumDetector::dataProcessingThread()
{
    printf("Data processing thread started\n");

    auto lastUpdateTime = std::chrono::steady_clock::now();
    auto lastRateUpdateTime = std::chrono::steady_clock::now();

    while (threadsRunning)
    {
        // Wait for data to be available for processing
        epicsEventWaitWithTimeout(dataAvailable, 1.0); // 1 second timeout

        auto currentTime = std::chrono::steady_clock::now();

        // Update count rates every second
        auto rateElapsed = std::chrono::duration_cast<std::chrono::seconds>(
            currentTime - lastRateUpdateTime).count();

        if (rateElapsed >= 1)
        {
            //updateCountRates();
            lastRateUpdateTime = currentTime;
        }

        // Update spectrum displays every 2 seconds
        auto displayElapsed = std::chrono::duration_cast<std::chrono::seconds>(
            currentTime - lastUpdateTime).count();

        if (displayElapsed >= 2)
        {
            //updateSpectra();
            lastUpdateTime = currentTime;
        }

        // Update MCA and TDC

        // Update EPICS parameters
        setIntegerParam(GermaniumSS, acquisitionRunning ? 1 : 0);
        setIntegerParam(GermaniumUS, framestat);

        // Update frame statistics
        static int lastFramestat = 0;
        if (framestat != lastFramestat)
        {
            setDoubleParam(GermaniumRATE, static_cast<double>(framestat - lastFramestat) / rateElapsed);
            lastFramestat = framestat;
        }

        // Call parameter callbacks
        callParamCallbacks();

        // Brief sleep to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    printf("Data processing thread stopped\n");
}

//===========================================================================//

/*
 * Add data to circular write buffer for file writing thread
 */
void germaniumDetector::addDataToWriteBuffer(const uint8_t* data, size_t dataSize)
{
    if (!data || dataSize == 0 || dataSize > DATA_WRITE_BUFFER_SIZE / 4)
    {
        return; // Data too large for buffer
    }
    
    // Take buffer mutex
    epicsMutexLock(writeBufferMutex);
    
    // Check if buffer has space
    if (writeBufferCount + dataSize + sizeof(size_t) > DATA_WRITE_BUFFER_SIZE)
    {
        epicsMutexUnlock(writeBufferMutex);
        printf("Write buffer full, dropping data packet\n");
        return;
    }
    
    // Add data size header
    size_t* sizePtr = reinterpret_cast<size_t*>(&dataWriteBuffer[writeBufferHead]);
    *sizePtr = dataSize;
    writeBufferHead = (writeBufferHead + sizeof(size_t)) % DATA_WRITE_BUFFER_SIZE;
    writeBufferCount += sizeof(size_t);
    
    // Add data - handle wrap-around
    if (writeBufferHead + dataSize <= DATA_WRITE_BUFFER_SIZE)
    {
        // No wrap-around
        memcpy(&dataWriteBuffer[writeBufferHead], data, dataSize);
        writeBufferHead = (writeBufferHead + dataSize) % DATA_WRITE_BUFFER_SIZE;
    }
    else
    {
        // Handle wrap-around
        size_t firstPart = DATA_WRITE_BUFFER_SIZE - writeBufferHead;
        memcpy(&dataWriteBuffer[writeBufferHead], data, firstPart);
        memcpy(&dataWriteBuffer[0], data + firstPart, dataSize - firstPart);
        writeBufferHead = dataSize - firstPart;
    }
    
    writeBufferCount += dataSize;
    
    epicsMutexUnlock(writeBufferMutex);
    
    // Signal data writing thread
    epicsEventSignal(dataWriteAvailable);
}

//===========================================================================//

/*
 * Data writing thread function (C wrapper)
 */
//extern "C"
void germaniumDetector::dataWriteThreadC(void *drvPvt)
{
    germaniumDetector *pGermanium = static_cast<germaniumDetector*>(drvPvt);
    pGermanium->dataWriteThread();
}

//===========================================================================//

/*
 * Data writing thread - handles file writing from buffer
 */
void germaniumDetector::dataWriteThread()
{
    printf("Data writing thread started\n");
    
    while (threadsRunning)
    {
        // Wait for data to be available
        epicsEventWaitWithTimeout(dataWriteAvailable, 1.0); // 1 second timeout
        
        // Process all available data in buffer
        while (writeBufferCount > 0 && threadsRunning)
        {
            epicsMutexLock(writeBufferMutex);
            
            if (writeBufferCount < sizeof(size_t))
            {
                epicsMutexUnlock(writeBufferMutex);
                break;
            }
            
            // Read data size
            size_t* sizePtr = reinterpret_cast<size_t*>(&dataWriteBuffer[writeBufferTail]);
            size_t dataSize = *sizePtr;
            writeBufferTail = (writeBufferTail + sizeof(size_t)) % DATA_WRITE_BUFFER_SIZE;
            writeBufferCount -= sizeof(size_t);
            
            if (writeBufferCount < dataSize)
            {
                // Corrupted buffer state
                printf("Write buffer corruption detected, resetting\n");
                writeBufferHead = writeBufferTail = writeBufferCount = 0;
                epicsMutexUnlock(writeBufferMutex);
                break;
            }
            
            // Create temporary buffer for data
            std::vector<uint8_t> tempBuffer(dataSize);
            
            // Read data - handle wrap-around
            if (writeBufferTail + dataSize <= DATA_WRITE_BUFFER_SIZE)
            {
                // No wrap-around
                memcpy(tempBuffer.data(), &dataWriteBuffer[writeBufferTail], dataSize);
                writeBufferTail = (writeBufferTail + dataSize) % DATA_WRITE_BUFFER_SIZE;
            }
            else
            {
                // Handle wrap-around
                size_t firstPart = DATA_WRITE_BUFFER_SIZE - writeBufferTail;
                memcpy(tempBuffer.data(), &dataWriteBuffer[writeBufferTail], firstPart);
                memcpy(tempBuffer.data() + firstPart, &dataWriteBuffer[0], dataSize - firstPart);
                writeBufferTail = dataSize - firstPart;
            }
            
            writeBufferCount -= dataSize;
            
            epicsMutexUnlock(writeBufferMutex);
            
            // Write data to file
            writeDataToFile(tempBuffer.data(), dataSize);
        }

        // Close the data file if the data in the buffer is the last in the current count
        //if ( last_data )
        //{
        //    closeCurrentDataFile();
        //}
    }
    
    printf("Data writing thread stopped\n");
}

//===========================================================================//

/*
 * Generate full filename based on DIR, FNAME, RUNNO, and segment number
 * Format: $(DIR)$(FNAME)-$(RUNNO)-$(SEGMENT-NO).bin
 */
std::string germaniumDetector::generateFilename( int seg_num, int run_num )
{
    char dir[256];
    char file_name[256];
    int runNumber;
    
    getStringParam( GermaniumDIR, sizeof(dir), dir );
    getStringParam( GermaniumFNAM, sizeof(file_name), file_name);
    
    // Ensure directory path ends with '/'
    std::string full_path( dir );
    if ( !full_path.empty() && full_path.back() != '/' )
    {
        full_path += '/';
    }
    
    // Generate full filename
    char full_file_name[512];
    snprintf(full_file_name, sizeof(full_file_name), "%s%s-%06d-%03d.bin",
             fullPath.c_str(), file_name, run_num, seg_num);
    
    return std::string(full_file_name);
}

//===========================================================================//

/*
 * Flush any remaining data in write buffer
 */
void germaniumDetector::flushWriteBuffer()
{
    printf("Flushing write buffer...\n");
    
    // Process remaining data
    while (writeBufferCount > 0)
    {
        epicsMutexLock(writeBufferMutex);
        
        if (writeBufferCount < sizeof(size_t))
        {
            writeBufferCount = 0;
            epicsMutexUnlock(writeBufferMutex);
            break;
        }
        
        // Read and write remaining data (similar to dataWriteThread)
        size_t* sizePtr = reinterpret_cast<size_t*>(&dataWriteBuffer[writeBufferTail]);
        size_t dataSize = *sizePtr;
        
        if (writeBufferCount < dataSize + sizeof(size_t))
        {
            writeBufferCount = 0;
            epicsMutexUnlock(writeBufferMutex);
            break;
        }
        
        writeBufferTail = (writeBufferTail + sizeof(size_t)) % DATA_WRITE_BUFFER_SIZE;
        writeBufferCount -= sizeof(size_t);
        
        std::vector<uint8_t> tempBuffer(dataSize);
        
        if (writeBufferTail + dataSize <= DATA_WRITE_BUFFER_SIZE)
        {
            memcpy(tempBuffer.data(), &dataWriteBuffer[writeBufferTail], dataSize);
            writeBufferTail = (writeBufferTail + dataSize) % DATA_WRITE_BUFFER_SIZE;
        }
        else
        {
            size_t firstPart = DATA_WRITE_BUFFER_SIZE - writeBufferTail;
            memcpy(tempBuffer.data(), &dataWriteBuffer[writeBufferTail], firstPart);
            memcpy(tempBuffer.data() + firstPart, &dataWriteBuffer[0], dataSize - firstPart);
            writeBufferTail = dataSize - firstPart;
        }
        
        writeBufferCount -= dataSize;
        
        epicsMutexUnlock(writeBufferMutex);
        
        writeDataToFile(tempBuffer.data(), dataSize);
    }
    
    printf("Write buffer flushed\n");
}

//===========================================================================//

void germaniumDetector::publish2DUInt32Array( const std::vector<uint32_t>& vec
                                            , size_t nx
                                            , size_t ny
                                            , int addr
                                            )
{
    size_t dims[2] = { nx, ny };
    NDArray* pArray = pNDArrayPool->alloc(2, dims, NDUInt32, 0, nullptr);
    if (!pArray)
    {
        return;
    }

    lock();

    memcpy(pArray->pData, vec.data(), nx * ny * sizeof(uint32_t));

    doCallbacksGenericPointer( pArray, NDArrayData, addr );

    unlock();

    pArray->release();
}

//===========================================================================//

void germaniumDetector::publish1DUInt32Array( std::span<const uint32_t> vec
                                            , int addr
                                            )
{
    size_t dims[1] = { vec.size() };
    NDArray* pArray = pNDArrayPool->alloc(1, dims, NDUInt32, 0, nullptr);
    if ( !pArray )
    {
        return;
    }

    lock();

    memcpy(pArray->pData, vec.data(), vec.size() * sizeof(uint32_t));

    doCallbacksGenericPointer( pArray, NDArrayData, addr );

    unlock();

    pArray->release();
}

//===========================================================================//

void germaniumDetector::publishMCA()
{
    publish2DUInt32Array( mca_data_, mca_nx_, mca_ny_, mca_addr_ );
}

//===========================================================================//

void germaniumDetector::publishTDC()
{
    publish2DUInt32Array( tdc_data_, tdc_nx_, tdc_ny_, tdc_addr_ );
}

//===========================================================================//

void germaniumDetector::publishSPCT()
{
    int monch;
    getIntegerParam( GermaniumMONCH, &monch );
    const size_t offset = static_cast<size_t>(monch) * mca_nx_;
    std::span<const uint32_t> spct(mca_data_.data() + offset, mca_nx_);
    publish1DUInt32Array( spct, spct_addr_ );
}

//===========================================================================//

void germaniumDetector::publishINTENS()
{
    std::span<const uint32_t> intens(intens_data_.data(), intens_data_.size() );
    publish1DUInt32Array( intens, intens_addr_ );
}

//===========================================================================//

void germaniumDetector::publishData()
{
    publishMCA();
    publishTDC();
    publishSPCT();
    publishINTENS();
}

//===========================================================================//

void germaniumDetector::dataProcessingThreadC(void *drvPvt)
{
    germaniumDetector *pGermanium = static_cast<germaniumDetector*>(drvPvt);
    pGermanium->dataProcessingThread();
}

//===========================================================================//

void germaniumDetector::dataProcessingThread()
{
    while(1)
    {
        uint32_t idx;
        if ( !rx2cmp_.pop(idx) )
        {
            // No data, sleep briefly
            std::this_thread::yield();
            continue;
        }
        Packet& p = pool_[idx];
        for ( int i=0; i<p.qw_len_; i++)
        {
            // Process data - placeholder
        }

        if ( p.finish_one())
        {
            pool_.release(idx);
        }
    }
}

//===========================================================================//

void germaniumDetector::dataWriteThreadC(void *drvPvt)
{
    germaniumDetector *pGermanium = static_cast<germaniumDetector*>(drvPvt);
    pGermanium->dataWriteThread();
}

//===========================================================================//

void germaniumDetector::dataWriteThread()
{
    int block_size, file_size;
    int fd;
    int seg_num = 0;
    int file_handle = -1;
    std::string file_name;

    // Large bufffer to hold data for writing
    std::vector<uint8_t> buffer;
    buffer.reserve(static_cast<size_t>(block_size_.load(std::memory_order_relaxed)) * 2048); // pre-reserve ~2× block

    while(1)
    {
        uint32_t runno;
        uint32_t idx;
        if ( !write2cmp_.pop(idx) )
        {
            // No data, sleep briefly
            std::this_thread::yield();
            continue;
        }
        Packet& p = pool_[idx];

        // Check if it's the first or last packet of a frame
        

        auto bytes = std::as_bytes( p.span_qw() );
        const uint8_t* src = reinterpret_cast<const uint8_t*>(bytes.data());
        buffer.insert( buffer.end(), src, src + bytes.size() );

        const size_t max_block_size = static_cast<size_t>( block_size_.load(std::memory_order_relaxed)) * 1024;

        if ( (max_block_size != 0) && (buffer.size() < max_block_size))
        {
            continue;
        }

        // Write buffer to file
        if ( file_handle < 0 )  // Need to open new file
        {
            std::string filename = generateFilename(seg_num);
            file_handle = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if ( file_handle < 0 )
            {
                errlogPrintf( "Failed to open data file: %s (error: %s)\n"
                            , filename.c_str()
                            , strerror(errno)
                            );
                pool_.release(idx);
                continue;
            }
            file_size = 0;
            file_name = filename;
            printf("Opened new data file: %s (segment %d)\n", 
                   filename.c_str(), seg_num);
        }

        // Write data to file - placeholder

        file_size += p.qw_len_ * sizeof(uint64_t);
        if ( file_size > static_cast<size_t>(file_size) )
        {
            seg_num = 0;
            close(file_handle);
        }
        else
        {
            seg_num++;
        }


        if ( p.finish_one())
        {
            pool_.release(idx);
        }
    }
}