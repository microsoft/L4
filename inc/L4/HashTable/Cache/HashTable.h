#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include "detail/ToRawPointer.h"
#include "Epoch/IEpochActionManager.h"
#include "HashTable/IHashTable.h"
#include "HashTable/ReadWrite/HashTable.h"
#include "HashTable/Cache/Metadata.h"
#include "Utils/Clock.h"

namespace L4
{
namespace HashTable
{
namespace Cache
{

// ReadOnlyHashTable class implements IReadOnlyHashTable interface and provides
// the functionality to read data given a key.
template <typename Allocator, typename Clock = Utils::EpochClock>
class ReadOnlyHashTable
    : public virtual ReadWrite::ReadOnlyHashTable<Allocator>
    , protected Clock
{
public:
    using Base = ReadWrite::ReadOnlyHashTable<Allocator>;

    class Iterator;

    ReadOnlyHashTable(
        HashTable& hashTable,
        std::chrono::seconds recordTimeToLive)
        : Base{
            hashTable,
            RecordSerializer{
                hashTable.m_setting.m_fixedKeySize,
                hashTable.m_setting.m_fixedValueSize,
                Metadata::c_metaDataSize } }
        , m_recordTimeToLive{ recordTimeToLive }
    {}

    virtual bool Get(const Key& key, Value& value) const override
    {
        const auto status = GetInternal(key, value);

        // Note that the following const_cast is safe and necessary to update cache hit information.
        const_cast<HashTablePerfData&>(GetPerfData()).Increment(
            status
            ? HashTablePerfCounter::CacheHitCount
            : HashTablePerfCounter::CacheMissCount);

        return status;
    }

    virtual IIteratorPtr GetIterator() const override
    {
        return std::make_unique<Iterator>(
            m_hashTable,
            m_recordSerializer,
            m_recordTimeToLive,
            GetCurrentEpochTime());
    }

    ReadOnlyHashTable(const ReadOnlyHashTable&) = delete;
    ReadOnlyHashTable& operator=(const ReadOnlyHashTable&) = delete;

protected:
    bool GetInternal(const Key& key, Value& value) const
    {
        if (!Base::Get(key, value))
        {
            return false;
        }

        assert(value.m_size > Metadata::c_metaDataSize);

        // If the record with the given key is found, check if the record is expired or not.
        // Note that the following const_cast is safe and necessary to update the access status.
        Metadata metaData{ const_cast<std::uint32_t*>(reinterpret_cast<const std::uint32_t*>(value.m_data)) };
        if (metaData.IsExpired(GetCurrentEpochTime(), m_recordTimeToLive))
        {
            return false;
        }

        metaData.UpdateAccessStatus(true);

        value.m_data += Metadata::c_metaDataSize;
        value.m_size -= Metadata::c_metaDataSize;

        return true;
    }

    std::chrono::seconds m_recordTimeToLive;
};


template <typename Allocator, typename Clock>
class ReadOnlyHashTable<Allocator, Clock>::Iterator : public Base::Iterator
{
public:
    using Base = typename Base::Iterator;

    Iterator(
        const HashTable& hashTable,
        const RecordSerializer& recordDeserializer,
        std::chrono::seconds recordTimeToLive,
        std::chrono::seconds currentEpochTime)
        : Base(hashTable, recordDeserializer)
        , m_recordTimeToLive{ recordTimeToLive }
        , m_currentEpochTime{ currentEpochTime }
    {}

    Iterator(Iterator&& other)
        : Base(std::move(other))
        , m_recordTimeToLive{ std::move(other.m_recordTimeToLive) }
        , m_currentEpochTime{ std::move(other.m_currentEpochTime) }
    {}

    bool MoveNext() override
    {
        if (!Base::MoveNext())
        {
            return false;
        }

        do
        {
            const Metadata metaData{
                const_cast<std::uint32_t*>(
                    reinterpret_cast<const std::uint32_t*>(
                        Base::GetValue().m_data)) };

            if (!metaData.IsExpired(m_currentEpochTime, m_recordTimeToLive))
            {
                return true;
            }
        } while (Base::MoveNext());

        return false;
    }

    Value GetValue() const override
    {
        auto value = Base::GetValue();
        value.m_data += Metadata::c_metaDataSize;
        value.m_size -= Metadata::c_metaDataSize;

        return value;
    }

private:
    std::chrono::seconds m_recordTimeToLive;
    std::chrono::seconds m_currentEpochTime;
};


// The following warning is from the virtual inheritance and safe to disable in this case.
// https://msdn.microsoft.com/en-us/library/6b3sy7ae.aspx
#pragma warning(push)
#pragma warning(disable:4250)

// WritableHashTable class implements IWritableHashTable interface and also provides
// the read only access (Get()) to the hash table.
template <typename Allocator, typename Clock = Utils::EpochClock>
class WritableHashTable
    : public ReadOnlyHashTable<Allocator, Clock>
    , public ReadWrite::WritableHashTable<Allocator>
{
public:
    using ReadOnlyBase = ReadOnlyHashTable<Allocator, Clock>;
    using WritableBase = typename ReadWrite::WritableHashTable<Allocator>;

    WritableHashTable(
        HashTable& hashTable,
        IEpochActionManager& epochManager,
        std::uint64_t maxCacheSizeInBytes,
        std::chrono::seconds recordTimeToLive,
        bool forceTimeBasedEviction)
        : ReadOnlyBase::Base(
            hashTable,
            RecordSerializer{
                hashTable.m_setting.m_fixedKeySize,
                hashTable.m_setting.m_fixedValueSize,
                Metadata::c_metaDataSize })
        , ReadOnlyBase(hashTable, recordTimeToLive)
        , WritableBase(hashTable, epochManager)
        , m_maxCacheSizeInBytes{ maxCacheSizeInBytes }
        , m_forceTimeBasedEviction{ forceTimeBasedEviction }
        , m_currentEvictBucketIndex{ 0U }
    {}

    using ReadOnlyBase::Get;
    using ReadOnlyBase::GetPerfData;

    virtual void Add(const Key& key, const Value& value) override
    {
        if (m_forceTimeBasedEviction)
        {
            EvictBasedOnTime(key);
        }

        Evict(key.m_size + value.m_size + Metadata::c_metaDataSize);

        WritableBase::Add(CreateRecordBuffer(key, value));
    }

    virtual ISerializerPtr GetSerializer() const override
    {
        throw std::exception("Not implemented yet.");
    }

private:
    using Mutex = std::mutex;
    using Lock = std::lock_guard<Mutex>;

    void EvictBasedOnTime(const Key& key)
    {
        const auto bucketIndex = GetBucketInfo(key).first;

        auto* entry = &m_hashTable.m_buckets[bucketIndex];

        const auto curEpochTime = GetCurrentEpochTime();

        HashTable::Lock lock{ m_hashTable.GetMutex(bucketIndex) };

        while (entry != nullptr)
        {
            for (std::uint8_t i = 0; i < HashTable::Entry::c_numDataPerEntry; ++i)
            {
                const auto data = entry->m_dataList[i].Load(std::memory_order_relaxed);

                if (data != nullptr)
                {
                    const Metadata metadata{
                        const_cast<std::uint32_t*>(
                            reinterpret_cast<const std::uint32_t*>(
                                m_recordSerializer.Deserialize(*data).m_value.m_data)) };

                    if (metadata.IsExpired(curEpochTime, m_recordTimeToLive))
                    {
                        WritableBase::Remove(*entry, i);
                        m_hashTable.m_perfData.Increment(HashTablePerfCounter::EvictedRecordsCount);
                    }
                }
            }

            entry = entry->m_next.Load(std::memory_order_relaxed);
        }
    }

    // Evict uses CLOCK algorithm to evict records based on expiration and access status
    // until the number of bytes freed match the given number of bytes needed.
    void Evict(std::uint64_t bytesNeeded)
    {
        std::uint64_t numBytesToFree = CalculateNumBytesToFree(bytesNeeded);
        if (numBytesToFree == 0U)
        {
            return;
        }

        // Start evicting records with a lock.
        Lock evictLock{ m_evictMutex };

        // Recalculate the number of bytes to free since other thread may have already evicted.
        numBytesToFree = CalculateNumBytesToFree(bytesNeeded);
        if (numBytesToFree == 0U)
        {
            return;
        }

        const auto curEpochTime = GetCurrentEpochTime();

        // The max number of iterations we are going through per eviction is twice the number
        // of buckets so that it can clear the access status. Note that this is the worst
        // case scenario and the eviction process should exit much quicker in a normal case.
        auto& buckets = m_hashTable.m_buckets;
        std::uint64_t numIterationsRemaining = buckets.size() * 2U;
        
        while (numBytesToFree > 0U && numIterationsRemaining-- > 0U)
        {
            const auto currentBucketIndex = m_currentEvictBucketIndex++ % buckets.size();
            auto& bucket = buckets[currentBucketIndex];

            // Lock the bucket since another thread can bypass Evict() since TotalDataSize can
            // be updated before the lock on m_evictMutex is released.
            HashTable::UniqueLock lock{ m_hashTable.GetMutex(currentBucketIndex) };
            HashTable::Entry* entry = &bucket;
        
            while (entry != nullptr)
            {
                for (std::uint8_t i = 0; i < HashTable::Entry::c_numDataPerEntry; ++i)
                {
                    const auto data = entry->m_dataList[i].Load(std::memory_order_relaxed);

                    if (data != nullptr)
                    {
                        const auto record = m_recordSerializer.Deserialize(*data);
                        const auto& value = record.m_value;

                        Metadata metadata{
                            const_cast<std::uint32_t*>(
                                reinterpret_cast<const std::uint32_t*>(
                                    value.m_data)) };

                        // Evict this record if
                        // 1: the record is expired, or
                        // 2: the entry is not recently accessed (and unset the access bit if set).
                        if (metadata.IsExpired(curEpochTime, m_recordTimeToLive)
                            || !metadata.UpdateAccessStatus(false))
                        {
                            const auto numBytesFreed = record.m_key.m_size + value.m_size;
                            numBytesToFree = (numBytesFreed >= numBytesToFree) ? 0U : numBytesToFree - numBytesFreed;

                            WritableBase::Remove(*entry, i);

                            m_hashTable.m_perfData.Increment(HashTablePerfCounter::EvictedRecordsCount);
                        }
                    }
                }

                entry = entry->m_next.Load(std::memory_order_relaxed);
            }
        }
    }

    // Given the number of bytes needed, it calculates the number of bytes
    // to free based on the max cache size.
    std::uint64_t CalculateNumBytesToFree(std::uint64_t bytesNeeded) const
    {
        const auto& perfData = GetPerfData();

        const std::uint64_t totalDataSize =
            perfData.Get(HashTablePerfCounter::TotalKeySize)
            + perfData.Get(HashTablePerfCounter::TotalValueSize)
            + perfData.Get(HashTablePerfCounter::TotalIndexSize);

        if ((bytesNeeded < m_maxCacheSizeInBytes)
            && (totalDataSize + bytesNeeded <= m_maxCacheSizeInBytes))
        {
            // There are enough free bytes.
            return 0U;
        }

        // (totalDataSize > m_maxCacheSizeInBytes) case is possible:
        // 1) If multiple threads are evicting and adding at the same time.
        //    For example, if thread A was evicting and thread B could have
        //    used the evicted bytes before thread A consumed.
        // 2) If max cache size is set lower than expectation.
        return (totalDataSize > m_maxCacheSizeInBytes)
            ? (totalDataSize - m_maxCacheSizeInBytes + bytesNeeded)
            : bytesNeeded;
    }

    RecordBuffer* CreateRecordBuffer(const Key& key, const Value& value)
    {
        const auto bufferSize = m_recordSerializer.CalculateBufferSize(key, value);
        auto buffer = Detail::to_raw_pointer(
            m_hashTable.GetAllocator<std::uint8_t>().allocate(bufferSize));

        std::uint32_t metaDataBuffer;
        Metadata{ &metaDataBuffer, GetCurrentEpochTime() };

        // 4-byte Metadata is inserted between key and value buffer.
        return m_recordSerializer.Serialize(
            key,
            value,
            Value{ reinterpret_cast<std::uint8_t*>(&metaDataBuffer), sizeof(metaDataBuffer) },
            buffer,
            bufferSize);
    }

    Mutex m_evictMutex;
    const std::uint64_t m_maxCacheSizeInBytes;
    const bool m_forceTimeBasedEviction;
    std::uint64_t m_currentEvictBucketIndex;
};

#pragma warning(pop)

} // namespace Cache
} // namespace HashTable
} // namespace L4