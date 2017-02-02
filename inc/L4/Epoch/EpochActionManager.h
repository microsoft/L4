#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

#include "IEpochActionManager.h"
#include "Utils/Lock.h"

namespace L4
{


// EpochActionManager provides functionalities to add actions at an epoch and to perform
// actions up to the given epoch.
class EpochActionManager
{
public:
    // "numActionQueues" indicates how many action containers there will be in order to
    // increase the throughput of registering an action. This will be re-calculated to
    // the next highest power of two so that the "&" operator can be used for accessing
    // the next queue.
    explicit EpochActionManager(std::uint8_t numActionQueues);

    // Adds an action at a given epoch counter.
    // This function is thread-safe.
    void RegisterAction(std::uint64_t epochCounter, IEpochActionManager::Action&& action);

    // Perform actions whose associated epoch counter value is less than
    // the given epoch counter value, and returns the number of actions performed.
    std::uint64_t PerformActions(std::uint64_t epochCounter);

    EpochActionManager(const EpochActionManager&) = delete;
    EpochActionManager& operator=(const EpochActionManager&) = delete;

private:
    using Mutex = Utils::CriticalSection;
    using Lock = std::lock_guard<Mutex>;

    using Actions = std::vector<IEpochActionManager::Action>;

    // The following structure needs to be sorted by the epoch counter.
    // If the performance of using std::map becomes an issue, we can revisit this.
    using EpochToActions = std::map<std::uint64_t, Actions>;

    using EpochToActionsWithLock = std::tuple<std::unique_ptr<Mutex>, EpochToActions>;

    // Run actions based on the configuration.
    void ApplyActions(Actions& actions);

    // Stores mapping from a epoch counter to actions to perform.
    std::vector<EpochToActionsWithLock> m_epochToActionsList;

    // Used to point to the next EpochToActions to simulate round-robin access.
    std::atomic<std::uint32_t> m_counter;
};


} // namespace L4