/**
 * @file Zmq.hpp
 * @brief ZMQ client interface for ADGermanium.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 04/04/2026
 * 
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#pragma once

//===========================================================================//

#include <type_traits>
#include <atomic>
#include <string>

#include <zmq.hpp>

//===========================================================================//

template<typename T>
concept ZmqMessage = std::is_trivially_copyable_v<T> &&
                     std::is_standard_layout_v<T>;

class ZmqClient
{
public:
    ZmqClient( zmq::context_t&    ctx
             , const std::string& txEndpoint
             , const std::string& rxEndpoint
             );

    enum class RecvStatus
    {
        Ok,
        Timeout,
        SizeMismatch
    };

    enum class ServerStatus
    {
        Normal,
        Down
    };

    template<ZmqMessage T>
    RecvStatus tx( const T& msg )
    {
        return send_raw( &msg, sizeof(T) );
    }

    template<ZmqMessage T>
    RecvStatus rx( T& msg )
    {
        return recv_raw( &msg, sizeof(T) );
    }

    void resetTxSocket();

private:

    zmq::context_t&   ctx_;

    const std::string txEndpoint_;
    const std::string rxEndpoint_;

    zmq::socket_t txSock_;
    zmq::socket_t rxSock_;

    std::atomic<bool> serverDown_{false};
    std::atomic<bool> needReset_{false};

    void initTxSocket();
    void initRxSocket();

    RecvStatus send_raw(const void* data, size_t size);
    RecvStatus recv_raw(void* data, size_t size);
};
