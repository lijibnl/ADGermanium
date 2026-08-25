#include <atomic>
#include <limits>

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::LockFreeBroadcastSPMC()
{
    for (size_t i = 0; i < ConsumerCount; ++i)
    {
        head_[i].value.store(0, std::memory_order_relaxed);
    }
    tail_.store(0, std::memory_order_relaxed);
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
bool LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::enqueue(const T& item)
{
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
        return false;
    }

    buffer_[currentTail & (Capacity - 1)] = item;
    tail_.store(currentTail + 1, std::memory_order_release);
    return true;
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
const T* LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::front(size_t consumerIndex) const
{
    if (consumerIndex >= ConsumerCount)
    {
        return nullptr; // Invalid consumer index
    }

    size_t currentHead = head_[consumerIndex].value.load(std::memory_order_relaxed);

    if (currentHead == tail_.load(std::memory_order_acquire))
    {
        return nullptr; // Buffer is empty for this consumer
    }

    return &buffer_[currentHead & (Capacity - 1)];
}

//===========================================================================//

template <typename T, size_t Capacity, size_t ConsumerCount>
void LockFreeBroadcastSPMC<T, Capacity, ConsumerCount>::pop(size_t consumerIndex)
{
    if (consumerIndex >= ConsumerCount)
    {
        return; // Invalid consumer index
    }

    size_t currentHead = head_[consumerIndex].value.load(std::memory_order_relaxed);

    if (currentHead == tail_.load(std::memory_order_acquire))
    {
        return; // Buffer is empty for this consumer
    }

    head_[consumerIndex].value.store(currentHead + 1, std::memory_order_release);
}

//===========================================================================//

