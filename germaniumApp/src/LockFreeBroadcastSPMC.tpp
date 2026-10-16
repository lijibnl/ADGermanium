#include <atomic>
#include <limits>
#include <iostream>
#include <print>

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::LockFreeBroadcastSPMC()
{
    reset();
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
void LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::reset()
{
    for (size_t i = 0; i < ConsumerCount; i++)
    {
        head_[i].value.store(0, std::memory_order_relaxed);
    }
    tail_.store(0, std::memory_order_relaxed);
    
    pushRequested.store(false, std::memory_order_relaxed);
    pushed.store(true, std::memory_order_relaxed);
    for (size_t i = 0; i < ConsumerCount; ++i)
    {
        popRequested[i].store(false, std::memory_order_relaxed);
        popped[i].store(true, std::memory_order_relaxed);
    }
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
T* LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::pushRequest()
{
    if (!pushed.load(std::memory_order_relaxed) )
    {
        std::cerr << __func__
                  << ": push requested without a prior push call\n";
        std::abort();
    }

    if (pushRequested.load(std::memory_order_relaxed))
    {
        std::cerr << __func__
                  << ": push requested with a prior pushRequest call\n";
        std::abort();
    }

    size_t currentTail = tail_.load(std::memory_order_relaxed);

    size_t min_head = std::numeric_limits<size_t>::max();

    for (size_t i = 0; i < ConsumerCount; ++i)
    {
        auto h = head_[i].value.load( std::memory_order_acquire );
        if (h < min_head)
        {
            min_head = h;
        }
    }

    if ((currentTail - min_head) >= Capacity)
    {
        std::println("[{}]: buffer is full", __func__);
        return nullptr;
    }

    pushRequested.store(true, std::memory_order_relaxed);
    pushed.store(false, std::memory_order_relaxed);

    return &buffer_[currentTail & (Capacity - 1)];
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
void LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::pushCancelRequest()
{
    if (!pushRequested.load(std::memory_order_relaxed))
    {
        std::cerr << __func__
                  << ": push cancel requested without a prior pushRequest call\n";
        std::abort();
    }

    pushRequested.store(false, std::memory_order_relaxed);
    pushed.store(true, std::memory_order_relaxed);

    return;
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
bool LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::push()
{
    if (!pushRequested.load(std::memory_order_relaxed))
    {
        std::cerr << __func__
                  << ": push requested without a prior pushRequest call\n";
        std::abort();
    }

    pushRequested.store(false, std::memory_order_relaxed);
    pushed.store(true, std::memory_order_relaxed);
    tail_.fetch_add(1, std::memory_order_release);

    return true;
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
const T* LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::popRequest(size_t consumerIndex)
{
    if (consumerIndex >= ConsumerCount)
    {
        std::cerr << __func__
                  << ": invalid consumer index\n";
        std::abort(); // Invalid consumer index
    }

    if ( !popped[consumerIndex].load(std::memory_order_relaxed) )
    {
        std::cerr << __func__
                  << ": pop requested without a prior pop call\n";
        std::abort();
    }

    if (popRequested[consumerIndex].load(std::memory_order_relaxed))
    {
        std::cerr << __func__
                  << ": pop requested with a prior popRequest call\n";
        std::abort();
    }   

    size_t currentHead = head_[consumerIndex].value.load(std::memory_order_relaxed);

    if (currentHead == tail_.load(std::memory_order_acquire))
    {
        return nullptr; // Buffer is empty for this consumer
    }

    popRequested[consumerIndex].store(true, std::memory_order_relaxed);
    popped[consumerIndex].store(false, std::memory_order_relaxed);

    return &buffer_[currentHead & (Capacity - 1)];
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
void LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::popCancelRequest(size_t consumerIndex)
{
    if (consumerIndex >= ConsumerCount)
    {
        std::cerr << __func__
                  << ": invalid consumer index\n";
        std::abort(); // Invalid consumer index
    }

    if (!popRequested[consumerIndex].load(std::memory_order_relaxed))
    {
        std::cerr << __func__
                  << ": pop cancel requested without a prior popRequest call\n";
        std::abort();
    }

    popRequested[consumerIndex].store(false, std::memory_order_relaxed);
    popped[consumerIndex].store(true, std::memory_order_relaxed);
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
bool LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::pop(size_t consumerIndex)
{
    if (consumerIndex >= ConsumerCount)
    {
        std::abort(); // Invalid consumer index
    }

    if (!popRequested[consumerIndex].load(std::memory_order_relaxed))
    {
        std::cerr << __func__
                  << ": pop called without a prior popRequest call\n";
        std::abort();
    }

    size_t currentHead = head_[consumerIndex].value.load(std::memory_order_relaxed);

    head_[consumerIndex].value.store(currentHead + 1, std::memory_order_release);

    popRequested[consumerIndex].store(false, std::memory_order_relaxed);
    popped[consumerIndex].store(true, std::memory_order_relaxed);

    size_t currentTail = tail_.load(std::memory_order_acquire);

    return (currentHead+1 < currentTail);
}

//===========================================================================//

