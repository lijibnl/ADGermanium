#pragma once

#include <atomic>
#include <cstddef>

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
class LockFreeBroadcastSPMC
{
    static_assert ( ((Capacity & (Capacity - 1)) == 0) && (ConsumerCount > 0)
                  , "Capacity must be a power of two and ConsumerCount must be greater than 0"
                  );

public:
    LockFreeBroadcastSPMC();

    bool enqueue( const T& item);

    const T* front( size_t consumerIndex ) const;
    void pop( size_t consumerIndex );

private:
    T buffer_[Capacity];

    struct alignas(64) AlignedAtomicHead {
        std::atomic<size_t> value{0};
    };

    alignas(64) AlignedAtomicHead head_[ConsumerCount];
    alignas(64) std::atomic<size_t> tail_;

};

//===========================================================================//

#include "LockFreeBroadcastSPMC.tpp"

