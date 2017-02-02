#pragma once

#include <boost/any.hpp>
#include <memory>
#include <vector>
#include "Epoch/IEpochActionManager.h"
#include "HashTable/Config.h"
#include "HashTable/ReadWrite/HashTable.h"
#include "HashTable/Cache/HashTable.h"
#include "Utils/Containers.h"
#include "Utils/Exception.h"

namespace L4
{
namespace LocalMemory
{

class HashTableManager
{
public:
    template <typename Allocator>
    std::size_t Add(
        const HashTableConfig& config,
        IEpochActionManager& epochActionManager,
        Allocator allocator)
    {
        if (m_hashTableNameToIndex.find(config.m_name) != m_hashTableNameToIndex.end())
        {
            throw RuntimeException("Same hash table name already exists.");
        }

        using namespace HashTable;

        using InternalHashTable = ReadWrite::WritableHashTable<Allocator>::HashTable;

        auto internalHashTable = std::make_shared<InternalHashTable>(
            InternalHashTable::Setting{
                config.m_setting.m_numBuckets,
                (std::max)(config.m_setting.m_numBucketsPerMutex.get_value_or(1U), 1U),
                config.m_setting.m_fixedKeySize.get_value_or(0U),
                config.m_setting.m_fixedValueSize.get_value_or(0U) },
            allocator);

        // TODO: Create from a serializer.

        const auto& cacheConfig = config.m_cache;
        auto hashTable = 
            cacheConfig
            ? std::make_unique<Cache::WritableHashTable<Allocator>>(
                *internalHashTable,
                epochActionManager,
                cacheConfig->m_maxCacheSizeInBytes,
                cacheConfig->m_recordTimeToLive,
                cacheConfig->m_forceTimeBasedEviction)
            : std::make_unique<ReadWrite::WritableHashTable<Allocator>>(
                *internalHashTable,
                epochActionManager);

        m_internalHashTables.emplace_back(std::move(internalHashTable));
        m_hashTables.emplace_back(std::move(hashTable));

        const auto newIndex = m_hashTables.size() - 1;

        m_hashTableNameToIndex.emplace(config.m_name, newIndex);

        return newIndex;
    }

    IWritableHashTable& GetHashTable(const char* name)
    {
        assert(m_hashTableNameToIndex.find(name) != m_hashTableNameToIndex.cend());
        return GetHashTable(m_hashTableNameToIndex.find(name)->second);
    }

    IWritableHashTable& GetHashTable(std::size_t index)
    {
        assert(index < m_hashTables.size());
        return *m_hashTables[index];
    }

private:
    Utils::StdStringKeyMap<std::size_t> m_hashTableNameToIndex;

    std::vector<boost::any> m_internalHashTables;
    std::vector<std::unique_ptr<IWritableHashTable>> m_hashTables;
};

} // namespace LocalMemory
} // namespace L4