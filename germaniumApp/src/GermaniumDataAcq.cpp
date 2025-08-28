/*
 * GermaniumDataAcq.cpp
 * Data acquisition, reception, and file writing functionality
 * Handles UDP data reception, buffering, and multi-segment file writing
 */

#include "Germanium.hpp"
#include "GermaniumTypes.hpp"
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <chrono>

/*
 * Data acquisition and file management initialization
 */
void Germanium::setupDataAcquisition()
{
    printf("Setting up data acquisition system...\n");
    
    // Initialize acquisition parameters
    acquisitionRunning = false;
    evttot = 0;
    framestat = 0;
    currentFileSize = 0;
    currentSegmentNumber = 0;
    currentFileHandle = -1;
    
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

/*
 * Create data directory if it doesn't exist
 */
void Germanium::createDataDirectory()
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

/*
 * Generate full filename based on DIR, FNAME, RUNNO, and segment number
 * Format: $(DIR)$(FNAME)-$(RUNNO)-$(SEGMENT-NO).bin
 */
std::string Germanium::generateFilename(int segmentNumber)
{
    char dirPath[256];
    char fileName[256];
    int runNumber;
    
    getStringParam(GermaniumDIR, sizeof(dirPath), dirPath);
    getStringParam(GermaniumFNAM, sizeof(fileName), fileName);
    getIntegerParam(GermaniumRUNNO, &runNumber);
    
    // Ensure directory path ends with '/'
    std::string fullPath(dirPath);
    if (!fullPath.empty() && fullPath.back() != '/')
    {
        fullPath += '/';
    }
    
    // Generate full filename
    char fullFilename[512];
    snprintf(fullFilename, sizeof(fullFilename), "%s%s-%06d-%03d.bin",
             fullPath.c_str(), fileName, runNumber, segmentNumber);
    
    return std::string(fullFilename);
}

/*
 * Open new data file for writing
 */
bool Germanium::openNewDataFile()
{
    // Close current file if open
    closeCurrentDataFile();
    
    // Generate new filename
    std::string filename = generateFilename(currentSegmentNumber);
    
    // Open new file
    currentFileHandle = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (currentFileHandle < 0)
    {
        printf("Failed to open data file: %s (error: %s)\n", 
               filename.c_str(), strerror(errno));
        return false;
    }
    
    currentFileSize = 0;
    currentFilename = filename;
    
    printf("Opened new data file: %s (segment %d)\n", 
           filename.c_str(), currentSegmentNumber);
    
    return true;
}

/*
 * Close current data file
 */
void Germanium::closeCurrentDataFile()
{
    if (currentFileHandle >= 0)
    {
        close(currentFileHandle);
        printf("Closed data file: %s (size: %ld bytes)\n", 
               currentFilename.c_str(), currentFileSize);
        
        currentFileHandle = -1;
        totalFilesWritten++;
        totalBytesWritten += currentFileSize;
        currentFileSize = 0;
    }
}

/*
 * Write data to current file, handling file size limits and segmentation
 */
bool Germanium::writeDataToFile(const uint8_t* data, size_t dataSize)
{
    if (!fileWritingEnabled || !data || dataSize == 0)
    {
        return false;
    }
    
    int maxFileSize;
    getIntegerParam(GermaniumFSIZE, &maxFileSize);
    
    // Check if we need a new file
    if (currentFileHandle < 0 || 
        (currentFileSize + dataSize) > static_cast<size_t>(maxFileSize))
    {
        if (currentFileHandle >= 0)
        {
            closeCurrentDataFile();
            currentSegmentNumber++;
        }
        
        if (!openNewDataFile())
        {
            return false;
        }
    }
    
    // Write data to file
    ssize_t bytesWritten = write(currentFileHandle, data, dataSize);
    if (bytesWritten != static_cast<ssize_t>(dataSize))
    {
        printf("Failed to write data to file: %s (error: %s)\n", 
               currentFilename.c_str(), strerror(errno));
        return false;
    }
    
    currentFileSize += dataSize;
    
    // Sync file periodically for data safety
    if ((currentFileSize % (1024*1024)) < dataSize) // Every ~1MB
    {
        fsync(currentFileHandle);
    }
    
    return true;
}

/*
 * Start data acquisition and file writing
 */
void Germanium::startDataAcquisition()
{
    if (acquisitionRunning)
    {
        printf("Data acquisition already running\n");
        return;
    }
    
    // Reset counters and state
    evttot = 0;
    framestat = 0;
    currentSegmentNumber = 0;
    totalBytesWritten = 0;
    totalFilesWritten = 0;
    
    // Create data directory
    createDataDirectory();
    
    // Enable file writing
    fileWritingEnabled = true;
    
    // Start acquisition
    acquisitionRunning = true;
    
    // Send start command to hardware
    udpRegisterWrite(STRT, 1);
    
    printf("Data acquisition started\n");
}

/*
 * Stop data acquisition and file writing
 */
void Germanium::stopDataAcquisition()
{
    if (!acquisitionRunning)
    {
        printf("Data acquisition not running\n");
        return;
    }
    
    // Send stop command to hardware
    udpRegisterWrite(STOP, 1);
    
    // Stop acquisition
    acquisitionRunning = false;
    fileWritingEnabled = false;
    
    // Close current data file
    closeCurrentDataFile();
    
    // Flush any remaining data in write buffer
    flushWriteBuffer();
    
    printf("Data acquisition stopped. Total: %ld bytes in %d files\n", 
           totalBytesWritten, totalFilesWritten);
}

/*
 * UDP data reception thread function (C wrapper)
 */
extern "C" void udpDataThreadC(void *drvPvt)
{
    Germanium *pGermanium = static_cast<Germanium*>(drvPvt);
    pGermanium->udpDataThread();
}

/*
 * UDP data reception thread - handles incoming data packets
 */
void Germanium::udpDataThread()
{
    printf("UDP data reception thread started\n");
    
    uint8_t receiveBuffer[UDP_BUFFER_SIZE];
    struct sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);
    
    while (threadsRunning)
    {
        // Wait for data with timeout
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(udpDataSocket, &readfds);
        timeout.tv_sec = 1;   // 1 second timeout
        timeout.tv_usec = 0;
        
        int result = select(udpDataSocket + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(udpDataSocket, &readfds))
        {
            // Receive data packet
            ssize_t bytesReceived = recvfrom(udpDataSocket, receiveBuffer, sizeof(receiveBuffer), 0,
                                           (struct sockaddr*)&senderAddr, &senderAddrLen);
            
            if (bytesReceived > 0)
            {
                // Process received data
                processReceivedData(receiveBuffer, static_cast<size_t>(bytesReceived));
            }
            else if (bytesReceived < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                printf("Error receiving UDP data: %s\n", strerror(errno));
            }
        }
        else if (result < 0 && errno != EINTR)
        {
            printf("Error in UDP data select: %s\n", strerror(errno));
        }
        
        // Brief pause to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    printf("UDP data reception thread stopped\n");
}

/*
 * Process received UDP data packet
 */
void Germanium::processReceivedData(const uint8_t* data, size_t dataSize)
{
    if (!data || dataSize == 0)
    {
        return;
    }
    
    // Update statistics
    framestat++;
    
    // Add data to write buffer if file writing is enabled
    if (fileWritingEnabled && acquisitionRunning)
    {
        addDataToWriteBuffer(data, dataSize);
    }
    
    // Process data for spectrum updates (if it's spectrum data)
    if (dataSize >= sizeof(DataPacketHeader))
    {
        const DataPacketHeader* header = reinterpret_cast<const DataPacketHeader*>(data);
        
        switch (header->packetType)
        {
        case PACKET_TYPE_SPECTRUM:
            processSpectrumData(data + sizeof(DataPacketHeader), 
                              dataSize - sizeof(DataPacketHeader));
            break;
            
        case PACKET_TYPE_EVENT:
            processEventData(data + sizeof(DataPacketHeader), 
                           dataSize - sizeof(DataPacketHeader));
            break;
            
        case PACKET_TYPE_STATUS:
            processStatusData(data + sizeof(DataPacketHeader), 
                            dataSize - sizeof(DataPacketHeader));
            break;
            
        default:
            // Unknown packet type - still write to file but don't process
            break;
        }
    }
    
    // Update parameter callbacks periodically
    if ((framestat % 100) == 0) // Every 100 frames
    {
        callParamCallbacks();
    }
}

/*
 * Add data to circular write buffer for file writing thread
 */
void Germanium::addDataToWriteBuffer(const uint8_t* data, size_t dataSize)
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

/*
 * Data writing thread function (C wrapper)
 */
extern "C" void dataWriteThreadC(void *drvPvt)
{
    Germanium *pGermanium = static_cast<Germanium*>(drvPvt);
    pGermanium->dataWriteThread();
}

/*
 * Data writing thread - handles file writing from buffer
 */
void Germanium::dataWriteThread()
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
    }
    
    printf("Data writing thread stopped\n");
}

/*
 * Flush any remaining data in write buffer
 */
void Germanium::flushWriteBuffer()
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
