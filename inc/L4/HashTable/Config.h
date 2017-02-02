#pragma once

#include <boost/optional.hpp>
#include <cassert>
#include <cstdint>
#include <chrono>
#include <memory>
#include "HashTable/IHashTable.h"
#include "Serialization/IStream.h"
#include "Utils/Properties.h"

namespace L4
{

// HashTableConfig struct.
struct HashTableConfig
{
    struct Setting
    {
        using KeySize = IReadOnlyHashTable::Key::size_type;
        using ValueSize = IReadOnlyHashTable::Value::size_type;

        explicit Setting(
            std::uint32_t numBuckets,
            boost::optional<std::uint32_t> numBucketsPerMutex = {},
            boost::optional<KeySize> fixedKeySize = {},
            boost::optional<ValueSize> fixedValueSize = {})
            : m_numBuckets{ numBuckets }
            , m_numBucketsPerMutex{ numBucketsPerMutex }
            , m_fixedKeySize{ fixedKeySize }
            , m_fixedValueSize{ fixedValueSize }
        {}

        std::uint32_t m_numBuckets;
        boost::optional<std::uint32_t> m_numBucketsPerMutex;
        boost::optional<KeySize> m_fixedKeySize;
        boost::optional<ValueSize> m_fixedValueSize;
    };

    struct Cache
    {
        Cache(
            std::uint64_t maxCacheSizeInBytes,
            std::chrono::seconds recordTimeToLive,
            bool forceTimeBasedEviction)
            : m_maxCacheSizeInBytes{ maxCacheSizeInBytes }
            , m_recordTimeToLive{ recordTimeToLive }
            , m_forceTimeBasedEviction{ forceTimeBasedEviction }
        {}

        std::uint64_t m_maxCacheSizeInBytes;
        std::chrono::seconds m_recordTimeToLive;
        bool m_forceTimeBasedEviction;
    };

    struct Serializer
    {
        using Properties = Utils::Properties;

        Serializer(
            std::shared_ptr<IStreamReader> streamReader = {},
            boost::optional<Properties> properties = {})
            : m_streamReader{ streamReader }
            , m_properties{ properties }
        {}

        std::shared_ptr<IStreamReader> m_streamReader;
        boost::optional<Properties> m_properties;
    };

    HashTableConfig(
        std::string name,
        Setting setting,
        boost::optional<Cache> cache = {},
        boost::optional<Serializer> serializer = {})
        : m_name{ std::move(name) }
        , m_setting{ std::move(setting) }
        , m_cache{ cache }
        , m_serializer{ serializer }
    {
        assert(m_setting.m_numBuckets > 0U
            || (m_serializer && (serializer->m_streamReader != nullptr)));
    }

    std::string m_name;
    Setting m_setting;
    boost::optional<Cache> m_cache;
    boost::optional<Serializer> m_serializer;
};

} // namespace L4
