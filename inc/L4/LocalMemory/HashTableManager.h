#pragma once

#include <boost/any.hpp>
#include <memory>
#include <vector>
#include "LocalMemory/Memory.h"
#include "Epoch/IEpochActionManager.h"
#include "HashTable/Config.h"
#include "HashTable/ReadWrite/HashTable.h"
#include "HashTable/ReadWrite/Serializer.h"
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

                const auto& cacheConfig = config.m_cache;
                const auto& serializerConfig = config.m_serializer;

                if (cacheConfig && serializerConfig)
                {
                    throw RuntimeException(
                        "Constructing cache hash table via serializer is not supported.");
                }

                using namespace HashTable;

                using InternalHashTable = typename ReadWrite::WritableHashTable<Allocator>::HashTable;
                using Memory = typename LocalMemory::Memory<Allocator>;

                Memory memory{ allocator };

                std::shared_ptr<InternalHashTable> internalHashTable = (serializerConfig && serializerConfig->m_stream != nullptr)
                    ? ReadWrite::Deserializer<Memory, InternalHashTable, ReadWrite::WritableHashTable>(
                        serializerConfig->m_properties.get_value_or(HashTableConfig::Serializer::Properties())).
                    Deserialize(
                        memory,
                        *(serializerConfig->m_stream))
                    : memory.template MakeUnique<InternalHashTable>(
                        typename InternalHashTable::Setting{
                    config.m_setting.m_numBuckets,
                    (std::max)(config.m_setting.m_numBucketsPerMutex.get_value_or(1U), 1U),
                    config.m_setting.m_fixedKeySize.get_value_or(0U),
                    config.m_setting.m_fixedValueSize.get_value_or(0U) },
                    memory.GetAllocator());

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
