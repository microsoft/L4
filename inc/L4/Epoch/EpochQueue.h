#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include "Interprocess/Container/Vector.h"
#include "Utils/Exception.h"
#include "Utils/Lock.h"

namespace L4
{

    // EpochQueue struct represents reference counts for each epoch.
    // Each value of the queue (fixed-size array) is the reference counts at an index,
    // where an index represents an epoch (time).
    template <
        typename TSharableLock,
        typename TExclusiveLock,
        typename Allocator = std::allocator<void>
    >
        struct EpochQueue
    {
        static_assert(
            std::is_same<typename TSharableLock::mutex_type, typename TExclusiveLock::mutex_type>::value,
            "mutex type should be the same");

    public:
        EpochQueue(
            std::uint64_t epochCounter,
            std::uint32_t queueSize,
            Allocator allocator = Allocator())
            : m_frontIndex{ epochCounter }
            , m_backIndex{ epochCounter }
            , m_mutexForBackIndex{}
            , m_refCounts{ queueSize, typename Allocator::template rebind<RefCount>::other(allocator) }
        {
            if (queueSize == 0U)
            {
                throw RuntimeException("Zero queue size is not allowed.");
            }
        }

        using SharableLock = TSharableLock;
        using ExclusiveLock = TExclusiveLock;
        using RefCount = std::atomic<std::uint32_t>;
        using RefCounts = Interprocess::Container::Vector<
            RefCount,
            typename Allocator::template rebind<RefCount>::other>;

        // The followings (m_frontIndex and m_backIndex) are 
        // accessed/updated only by the owner thread (only one thread), thus
        // they don't require any synchronization.
        std::size_t m_frontIndex;

        // Back index represents the latest epoch counter value. Note that 
        // this is accessed/updated by multiple threads, thus requires 
        // synchronization.
        std::size_t m_backIndex;

        // Read/Write lock for m_backIndex.
        typename SharableLock::mutex_type m_mutexForBackIndex;

        // Reference counts per epoch count.
        // The index represents the epoch counter value and the value represents the reference counts.
        RefCounts m_refCounts;
    };


    // EpochRefManager provides functionality of adding/removing references
    // to the epoch counter.
    template <typename EpochQueue>
    class EpochRefManager
    {
    public:
        explicit EpochRefManager(EpochQueue& epochQueue)
            : m_epochQueue(epochQueue)
        {}

        // Increment a reference to the current epoch counter.
        // This function is thread-safe.
        std::uint64_t AddRef()
        {
            // The synchronization is needed for EpochCounterManager::AddNewEpoch().
            typename EpochQueue::SharableLock lock(m_epochQueue.m_mutexForBackIndex);

            ++m_epochQueue.m_refCounts[m_epochQueue.m_backIndex % m_epochQueue.m_refCounts.size()];

            return m_epochQueue.m_backIndex;
        }


        // Decrement a reference count for the given epoch counter.
        // This function is thread-safe.
        void RemoveRef(std::uint64_t epochCounter)
        {
            auto& refCounter = m_epochQueue.m_refCounts[epochCounter % m_epochQueue.m_refCounts.size()];

            if (refCounter == 0)
            {
                throw RuntimeException("Reference counter is invalid.");
            }

            --refCounter;
        }

        EpochRefManager(const EpochRefManager&) = delete;
        EpochRefManager& operator=(const EpochRefManager&) = delete;

    private:
        EpochQueue& m_epochQueue;
    };


    // EpochCounterManager provides functionality of updating the current epoch counter
    // and getting the latest unreferenced epoch counter.
    template <typename EpochQueue>
    class EpochCounterManager
    {
    public:
        explicit EpochCounterManager(EpochQueue& epochQueue)
            : m_epochQueue(epochQueue)
        {}

        // Increments the current epoch count by one.
        // This function is thread-safe.
        void AddNewEpoch()
        {
            // The synchronization is needed for EpochRefManager::AddRef().
            typename EpochQueue::ExclusiveLock lock(m_epochQueue.m_mutexForBackIndex);

            ++m_epochQueue.m_backIndex;

            // TODO: check for the overwrap and throw.
        }

        // Returns the epoch count in the queue where it is the biggest epoch
        // count such that all other epoch counts' references are zeros.
        // Note that this function is NOT thread safe, and should be run on the
        // same thread as the one that calls AddNewEpoch().
        std::uint64_t RemoveUnreferenceEpochCounters()
        {
            while (m_epochQueue.m_backIndex > m_epochQueue.m_frontIndex)
            {
                if (m_epochQueue.m_refCounts[m_epochQueue.m_frontIndex % m_epochQueue.m_refCounts.size()] == 0U)
                {
                    ++m_epochQueue.m_frontIndex;
                }
                else
                {
                    // There are references to the front of the queue and will return this front index.
                    break;
                }
            }

            return m_epochQueue.m_frontIndex;
        }

        EpochCounterManager(const EpochCounterManager&) = delete;
        EpochCounterManager& operator=(const EpochCounterManager&) = delete;

    private:
        EpochQueue& m_epochQueue;
    };

} // namespace L4
