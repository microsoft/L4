#pragma once

#include "Epoch/EpochRefPolicy.h"
#include "EpochManager.h"
#include "HashTableManager.h"

namespace L4 {
namespace LocalMemory {

class Context : private EpochRefPolicy<EpochManager::TheEpochRefManager> {
 public:
  Context(HashTableManager& hashTableManager,
          EpochManager::TheEpochRefManager& epochRefManager)
      : EpochRefPolicy<EpochManager::TheEpochRefManager>(epochRefManager),
        m_hashTableManager{hashTableManager} {}

  Context(Context&& context)
      : EpochRefPolicy<EpochManager::TheEpochRefManager>(std::move(context)),
        m_hashTableManager{context.m_hashTableManager} {}

  const IReadOnlyHashTable& operator[](const char* name) const {
    return m_hashTableManager.GetHashTable(name);
  }

  IWritableHashTable& operator[](const char* name) {
    return m_hashTableManager.GetHashTable(name);
  }

  const IReadOnlyHashTable& operator[](std::size_t index) const {
    return m_hashTableManager.GetHashTable(index);
  }

  IWritableHashTable& operator[](std::size_t index) {
    return m_hashTableManager.GetHashTable(index);
  }

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

 private:
  HashTableManager& m_hashTableManager;
};

}  // namespace LocalMemory
}  // namespace L4
