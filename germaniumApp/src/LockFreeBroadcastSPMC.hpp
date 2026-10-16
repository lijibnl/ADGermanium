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

    void reset();

    T* pushRequest();
    void pushCancelRequest();
    bool push();

    const T* popRequest( size_t consumerIndex );
    void popCancelRequest( size_t consumerIndex );
    bool pop( size_t consumerIndex );

private:
    T buffer_[Capacity];

    struct alignas(64) AlignedAtomicHead {
        std::atomic<size_t> value{0};
    };

    alignas(64) AlignedAtomicHead head_[ConsumerCount];
    alignas(64) std::atomic<size_t> tail_;

    std::atomic<bool> pushRequested, pushed;
    std::atomic<bool> popRequested[ConsumerCount], popped[ConsumerCount];

};

//===========================================================================//

#include "LockFreeBroadcastSPMC.tpp"

