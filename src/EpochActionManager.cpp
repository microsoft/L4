#include "Epoch/EpochActionManager.h"
#include "Utils/Math.h"

#include <cassert>
#include <memory>
#include <thread>

namespace L4 {

// EpochActionManager class implementation.

EpochActionManager::EpochActionManager(std::uint8_t numActionQueues)
    : m_epochToActionsList{}, m_counter{} {
  // Calculate numActionQueues as the next highest power of two.
  std::uint16_t newNumActionQueues = numActionQueues;
  if (numActionQueues == 0U) {
    newNumActionQueues =
        static_cast<std::uint16_t>(std::thread::hardware_concurrency());
  }
  newNumActionQueues = static_cast<std::uint16_t>(
      Utils::Math::NextHighestPowerOfTwo(newNumActionQueues));

  assert(newNumActionQueues != 0U &&
         Utils::Math::IsPowerOfTwo(newNumActionQueues));

  // Initialize m_epochToActionsList.
  m_epochToActionsList.resize(newNumActionQueues);
  for (auto& epochToActions : m_epochToActionsList) {
    std::get<0>(epochToActions) = std::make_unique<Mutex>();
  }
}

void EpochActionManager::RegisterAction(std::uint64_t epochCounter,
                                        IEpochActionManager::Action&& action) {
  std::uint32_t index = ++m_counter & (m_epochToActionsList.size() - 1);
  auto& epochToActions = m_epochToActionsList[index];

  Lock lock(*std::get<0>(epochToActions));
  std::get<1>(epochToActions)[epochCounter].emplace_back(std::move(action));
}

std::uint64_t EpochActionManager::PerformActions(std::uint64_t epochCounter) {
  // Actions will be moved here and performed without a lock.
  Actions actionsToPerform;

  for (auto& epochToActionsWithLock : m_epochToActionsList) {
    Lock lock(*std::get<0>(epochToActionsWithLock));

    // lower_bound() so that it is deleted up to but not including epochCounter.
    auto& epochToActions = std::get<1>(epochToActionsWithLock);
    const auto endIt = epochToActions.lower_bound(epochCounter);

    auto it = epochToActions.begin();

    while (it != endIt) {
      actionsToPerform.insert(actionsToPerform.end(),
                              std::make_move_iterator(it->second.begin()),
                              std::make_move_iterator(it->second.end()));

      // The following post increment is intentional to avoid iterator
      // invalidation issue.
      epochToActions.erase(it++);
    }
  }

  ApplyActions(actionsToPerform);

  return actionsToPerform.size();
}

void EpochActionManager::ApplyActions(Actions& actions) {
  for (auto& action : actions) {
    action();
  }
}

}  // namespace L4
