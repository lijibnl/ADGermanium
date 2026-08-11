/**
 * @file Zmq.cpp
 * @brief ZMQ client implementation for ADGermanium.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/04/2026
 * 
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#include <cstring>
#include <string>
#include <iostream>
#include "Zmq.hpp"

//===========================================================================//

ZmqClient::ZmqClient( zmq::context_t&    ctx
                    , const std::string& txEndpoint
                    , const std::string& rxEndpoint
                    )
                    : ctx_       ( ctx        )
                    , txEndpoint_( txEndpoint )
                    , rxEndpoint_( rxEndpoint )
{
    initTxSocket();
    initRxSocket();
}

//===========================================================================//

void ZmqClient::initTxSocket()
{
    txSock_ = zmq::socket_t( ctx_, zmq::socket_type::push );

    int linger = 0;
    txSock_.set(zmq::sockopt::linger, linger);

    int timeout = 1000;
    txSock_.set(zmq::sockopt::sndtimeo, timeout);
    
    try
    {
        {
            txSock_.connect( txEndpoint_ );            
        }
    }
    catch(const zmq::error_t& e)
    {
        std::cerr << "ZMQ Tx socket connect error: " << e.what() << '\n';
    }
}

//===========================================================================//

void ZmqClient::initRxSocket()
{
    rxSock_ = zmq::socket_t( ctx_, zmq::socket_type::pull );

    int linger = 0;
    rxSock_.set(zmq::sockopt::linger, linger);
    
    int timeout = 1000;    
    rxSock_.set(zmq::sockopt::rcvtimeo, timeout);
        
    try
    {
        {
            rxSock_.connect( rxEndpoint_ );            
        }
    }
    catch(const zmq::error_t& e)
    {
        std::cerr << "ZMQ Rx socket connect error: " << e.what() << '\n';
    }
}

//===========================================================================//

void ZmqClient::resetTxSocket()
{

    txSock_.close();
   
    initTxSocket();
}

//===========================================================================//

ZmqClient::RecvStatus ZmqClient::send_raw(const void* data, size_t size)
{
    zmq::message_t msg(size);
    std::memcpy(msg.data(), data, size);

    auto res = txSock_.send( msg, zmq::send_flags::none );
    return res.has_value() ? RecvStatus::Ok : RecvStatus::Timeout;
}

//===========================================================================//

ZmqClient::RecvStatus ZmqClient::recv_raw(void* data, size_t size)
{
    zmq::message_t msg;

    if ( !rxSock_.recv( msg, zmq::recv_flags::none) )
        return RecvStatus::Timeout;

    if ( msg.size() != size )
    {
        std::cerr << "ZMQ Rx socket received message of unexpected size: " << msg.size() << " (expected " << size << ")\n";
        return RecvStatus::SizeMismatch;
    }

    std::memcpy(data, msg.data(), size);

    return RecvStatus::Ok;
}

//===========================================================================//
