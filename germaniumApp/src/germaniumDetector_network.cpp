#include "germaniumDetector.hpp"

SOCKET GermaniumDetector::make_udp_bind(int port)
{
    SOCKET s = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    if(s == INVALID_SOCKET)
        return s;
    
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);

    if(bind(s, (sockaddr*)&a, sizeof(a)) < 0)
    {
        errlogSevPrintf(errlogMajor, "bind failed on %d\n", port);
        epicsSocketDestroy(s);
        return INVALID_SOCKET;
    }
    
    return s;
}

void Germanium::set_nonblock( SOCKET s )
{
    int fl = fncntl( s, F_GETFL, 0 );
    fcntl( s, F_SETFL, fl|O_NONBLOCK );
}


void GermaniumDetectorDriver::ctrlRxTask()
{
    while(running_)
    {
        UdpRxMsg rx{}; sockaddr_in from{}; socklen_t flen=sizeof(from);
        int n = recvfrom(ctrlSock_, (char*)&rx, sizeof(rx), 0, (sockaddr*)&from, &flen);
        if( n == (int)sizeof(rx) )
        {
            uint16_t op  = ntohs(rx.op);
            uint32_t val = ntohl(rx.val);

            if(op==OP_TEMPERATURE)
            {
                float temp;
                memcpy( &temp, &val, sizeof(float) );       // interpret val as IEEE-754 float32
                setDoubleParam(P_Temperature, (double)temp);
                callParamCallbacks();                       // triggers I/O Intr updates on ai record
            }
            // Handle other control ops (ACK, etc.) as needed
        }
        epicsThreadSleep(0.005);
    }
}


// Data processing: calculate MCA and TDC and publish
void UDPDetectorDriver::processAndPublish( const uint16_t* src
                                         , size_t nx
                                         , size_t ny
                                         )
{
    constexpr size_t nels = NX * NY;
    std::vector<uint16_t> buf(nels, 0);
  
    constexpr size_t copy = std::min(nels, nx*ny);
    if(copy > 0)
        memcpy( buf.data(), src, copy*sizeof(uint16_t) );
  
    // Post DATA waveform (asynInt16Array)
    doCallbacksInt16Array(reinterpret_cast<epicsInt16*>(buf.data()), nels, P_Data, 0);
  
    // Publish as NDArray so AD plugins can consume it
    size_t dims[2] = {NX, NY};
    NDArray* pArray = this->pNDArrayPool->alloc( 2, dims, NDUInt16, nels*sizeof(uint16_t), nullptr );
    memcpy( pArray->pData, buf.data(), nels*sizeof(uint16_t) );
    epicsTimeGetCurrent( &pArray->epicsTS );
    doCallbacksGenericPointer( pArray, NDArrayData, 0 );
    pArray->release();
}



void GermaniumDetectorDriver::dataRxTask()
{
    constexpr size_t MAX = 4096 + sizeof(UdpDataHdr); // adjust if your packets are larger
    std::vector<char> pkt(MAX);
    std::vector<uint16_t> frame(NX*NY);

    constexpr socklen_t flen=sizeof(from);

    while(running_)
    {
        sockaddr_in from{};
        
        int n = recvfrom(dataSock_, pkt.data(), (int)pkt.size(), 0, (sockaddr*)&from, &flen);
        if( n >= (int)sizeof(UdpDataHdr) )
        {
            auto* hdr = reinterpret_cast<UdpDataHdr*>(pkt.data());
            uint16_t op     = ntohs(hdr->op);
            uint32_t nbytes = ntohl(hdr->nbytes);

            if(op==OP_DATA && nbytes <= pkt.size() - sizeof(UdpDataHdr))
            {
                size_t ns = nbytes / sizeof(uint16_t);
                constexpr uint16_t* payload = reinterpret_cast<const uint16_t*>( pkt.data()+sizeof(UdpDataHdr) );
                size_t copy = std::min(frame.size(), ns);
                memcpy(frame.data(), payload, copy*sizeof(uint16_t));

                // Dummy processing hook: replace/extend with real algorithm
                processAndPublish(frame.data(), NX, NY);
      }
    }
    epicsThreadSleep(0.001);
  }
}

