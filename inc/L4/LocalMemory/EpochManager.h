#pragma once

#include <atomic>
#include <boost/thread/shared_lock_guard.hpp>
#include <mutex>
#include "Epoch/Config.h"
#include "Epoch/EpochActionManager.h"
#include "Epoch/EpochQueue.h"
#include "Log/PerfCounter.h"
#include "Utils/Lock.h"
#include "Utils/RunningThread.h"

namespace L4
{
namespace LocalMemory
{

// EpochManager aggregates epoch-related functionalities such as adding/removing
// client epoch queues, registering/performing actions, and updating the epoch counters.
class EpochManager : public IEpochActionManager
{
public:
    using TheEpochQueue = EpochQueue<
        boost::shared_lock_guard<Utils::ReaderWriterLockSlim>,
        std::lock_guard<Utils::ReaderWriterLockSlim>>;

    using TheEpochRefManager = EpochRefManager<TheEpochQueue>;

    EpochManager(
        const EpochManagerConfig& config,
        ServerPerfData& perfData)
        : m_perfData{ perfData }
        , m_config{ config }
        , m_currentEpochCounter{ 0U }
        , m_epochQueue{
            m_currentEpochCounter,
            m_config.m_epochQueueSize }
        , m_epochRefManager{ m_epochQueue }
        , m_epochCounterManager{ m_epochQueue }
        , m_epochActionManager{ config.m_numActionQueues }
        , m_processingThread{
            m_config.m_epochProcessingInterval,
            [this]
            {
                this->Remove();
                this->Add();
            }}
    {}

    TheEpochRefManager& GetEpochRefManager()
    {
        return m_epochRefManager;
    }

    void RegisterAction(Action&& action) override
    {
        m_epochActionManager.RegisterAction(m_currentEpochCounter, std::move(action));
        m_perfData.Increment(ServerPerfCounter::PendingActionsCount);
    }

    EpochManager(const EpochManager&) = delete;
    EpochManager& operator=(const EpochManager&) = delete;

private:
    using TheEpochCounterManager = EpochCounterManager<TheEpochQueue>;

    using ProcessingThread = Utils::RunningThread<std::function<void()>>;

    // Enqueues a new epoch whose counter value is last counter + 1.
    // This is called from the server side.
    void Add()
    {
        // Incrementing the global epoch counter before incrementing per-connection
        // epoch counter is safe (not so the other way around). If the server process is 
        // registering an action at the m_currentEpochCounter in RegisterAction(),
        // it is happening in the "future," and this means that if the client is referencing
        // the memory to be deleted in the "future," it will be safe.
        ++m_currentEpochCounter;

        m_epochCounterManager.AddNewEpoch();
    }

    // Dequeues any epochs whose ref counter is 0, meaning there is no reference at that time.
    void Remove()
    {
        const auto oldestEpochCounter = m_epochCounterManager.RemoveUnreferenceEpochCounters();

        const auto numActionsPerformed = m_epochActionManager.PerformActions(oldestEpochCounter);

        m_perfData.Subtract(ServerPerfCounter::PendingActionsCount, numActionsPerformed);
        m_perfData.Set(ServerPerfCounter::LastPerformedActionsCount, numActionsPerformed);
        m_perfData.Set(ServerPerfCounter::OldestEpochCounterInQueue, oldestEpochCounter);
        m_perfData.Set(ServerPerfCounter::LatestEpochCounterInQueue, m_currentEpochCounter);
    }

    // Reference to the performance data.
    ServerPerfData& m_perfData;

    // Configuration related to epoch manager.
    EpochManagerConfig m_config;

    // The global current epoch counter.
#if defined(_MSC_VER)
    std::atomic_uint64_t m_currentEpochCounter;
#else
    std::atomic<std::uint64_t> m_currentEpochCounter;
#endif

    // Epoch queue.
    TheEpochQueue m_epochQueue;

    // Handles adding/decrementing ref counts.
    TheEpochRefManager m_epochRefManager;

    // Handles adding new epoch and finding the epoch counts that have zero ref counts.
    TheEpochCounterManager m_epochCounterManager;

    // Handles registering/performing actions.
    EpochActionManager m_epochActionManager;

    // Thread responsible for updating the current epoch counter,
    // removing the unreferenced epoch counter, etc.
    // Should be the last member so that it gets destroyed first.
    ProcessingThread m_processingThread;
};

} // namespace LocalMemory
} // namespace L4
