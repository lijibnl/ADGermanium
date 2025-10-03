#include <atomic>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <string>
#include <vector>
#include <mutex>
#include <stop_token>
#include <span>

constexpr size_t BUF_QWORDS = 256;    // max packet size in qwords
constexpr size_t POOL_SIZE  = 8192;   // number of packet buffers
constexpr size_t Q_CAP      = 8192;   // SPSC ring size (power of two)

// SPCT ring
template< typename T, size_t CAP >
class SPSCQueue
{
    static_assert( (CAP & (CAP - 1)) == 0, "CAP must be power of two" );

public:
    bool push( const T& v)
    {
        size_t h = head_.load( std::memory_order_relaxed );
        size_t n = (h + 1) & mask_;
        if ( n == tail_.load(std::memory_order_acquire) ) return false;
        buff_[h] = v;
        head_.store( n, std::memory_order_release );
        return true;
    }

    bool pop( T& out )
    {
        size_t t = tail_.load(std::memory_order_relaxed);
        if ( t== head_.load(std::memory_order_acquire) ) return false;
        out = buff_[t];
        tail_.store( (t + 1) & mask_, std::memory_order_release );
        return true;
    }

    size_t size() const
    {
        size_t h = head_.load( std::memory_order_acquire );
        size_t t = tail_.load( std::memory_order_acquire );
        return (h >= t) ? (h - t) : (CAP - (t - h);
    }

private:
    static constexpr size_t mask_ = CAP - 1;
    std::array<T, CAP> buff_{};
    std::atomic<size_t> head_{0}, tail_{0};

    bool push(
};

//===========================================================================//

class Packet
{
public:
    Packet(): refcnt_(0), qw_len_(0){}

    // Write
    uint8_t* data_mut_bytes() noexcept
    {
        return reinterpret_cast<uint8_t*>(data_.data());
    }

    uint64_t* data_mut_qw() noexcept
    {
        return data_.data();
    }

    void set_qw_len(uint32_t n) noexcept
    {
        qw_len_ = n;
    }

    // Read
    std::span<const uint64_t> span_qw() const noexcept
    {
        return {data_.data(), qw_len_};
    }

    uint32_t qw_len() const noexcept
    {
        return qw_len_;
    }

    void set_refcount(uint32_t n) noexcept
    {
        refcnt.store(n, std::memory_order_release);
    }

    bool finish_one() noexcept
    {
        return refcnt_.fetch_sub( 1, std::memory_order_acq_rel) == 1;
    }

    bool in_use() const noexcept
    {
        return refcnt_.load( std::memory_order_acquire) != 0;
    }

private:
    std::atomic<uint32_t> refcnt_;
    uint32_t qw_len_;
    aliangas(64) std::array<uint64_t, BUF_QWORDS> data_;
};

template<typename T>
class BufferPool
{
public:
    BufferPool()
    {
        pool_.resize(POOL_SIZE);
        free_.reserve(POOL_SIZE);
        for( uint32_t i = 0; i < POOL_SIZE; ++i )
        {
            free.push_back(i);
        }
    }

    bool acquire( uint32_t& idx )
    {
        std::lock_guard lk(mtx_);
        if ( free_.empty() )
            return false;

        idx = free_.back();
        free_.pop_back();

        return true;
    }

    void release( uint32_t idx )
    {
        std::lock_guard lk(mtx_);
        free_.push_back(idx);
    }

    T& operator[](uint32_t) noexcept
    {
        return pool_[i];
    }

    const T& operator[](uint32_t) const noexcept
    {
        return pool_[i];
    }

private:
    std::mutex mtx_;
    std::vector<uint32_t> free_;
    std::vector<T> pool_;
};

//===========================================================================//

