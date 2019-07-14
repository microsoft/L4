#pragma once

#include <chrono>
#include <cstdint>

namespace L4 {

// EpochManagerConfig struct.
struct EpochManagerConfig {
  // "numActionQueues" indicates how many action containers there will be in
  // order to increase the throughput of registering an action.
  // "performActionsInParallelThreshold" indicates the threshold value above
  // which the actions are performed in parallel.
  // "maxNumThreadsToPerformActions" indicates how many threads will be used
  // when performing an action in parallel.
  explicit EpochManagerConfig(
      std::uint32_t epochQueueSize = 1000,
      std::chrono::milliseconds epochProcessingInterval =
          std::chrono::milliseconds{1000},
      std::uint8_t numActionQueues = 1)
      : m_epochQueueSize{epochQueueSize},
        m_epochProcessingInterval{epochProcessingInterval},
        m_numActionQueues{numActionQueues} {}

  std::uint32_t m_epochQueueSize;
  std::chrono::milliseconds m_epochProcessingInterval;
  std::uint8_t m_numActionQueues;
};

}  // namespace L4
