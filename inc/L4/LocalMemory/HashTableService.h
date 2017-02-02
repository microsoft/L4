#pragma once

#include "Context.h"
#include "EpochManager.h"
#include "HashTable/Config.h"
#include "Log/PerfCounter.h"

namespace L4
{
namespace LocalMemory
{

class HashTableService
{
public:
    explicit HashTableService(
        const EpochManagerConfig& epochManagerConfig = EpochManagerConfig())
        : m_epochManager{ epochManagerConfig, m_serverPerfData }
    {}

    template <typename Allocator = std::allocator<void>>
    std::size_t AddHashTable(
        const HashTableConfig& config,
        Allocator allocator = Allocator())
    {
        return m_hashTableManager.Add(config, m_epochManager, allocator);
    }

    Context GetContext()
    {
        return Context(m_hashTableManager, m_epochManager.GetEpochRefManager());
    }

private:
    ServerPerfData m_serverPerfData;

    HashTableManager m_hashTableManager;

    // Make sure HashTableManager is destroyed before EpochManager b/c
    // it is possible that EpochManager could be processing Epoch Actions
    // on hash tables.
    EpochManager m_epochManager;
};

} // namespace LocalMemory
} // namespace L4