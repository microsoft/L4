#pragma once

#include <boost/optional.hpp>
#include <cstdint>
#include <mutex>
#include "detail/ToRawPointer.h"
#include "Epoch/IEpochActionManager.h"
#include "HashTable/Common/SharedHashTable.h"
#include "HashTable/Common/Record.h"
#include "HashTable/IHashTable.h"
#include "HashTable/ReadWrite/Serializer.h"
#include "Log/PerfCounter.h"
#include "Utils/Exception.h"
#include "Utils/MurmurHash3.h"
#include "Utils/Properties.h"

namespace L4
{

// ReadWriteHashTable is a general purpose hash table where the look up is look free.
namespace HashTable
{
namespace ReadWrite
{

// ReadOnlyHashTable class implements IReadOnlyHashTable interface and provides
// the functionality to read data given a key.
template <typename Allocator>
class ReadOnlyHashTable : public virtual IReadOnlyHashTable
{
public:
    using HashTable = SharedHashTable<RecordBuffer, Allocator>;

    class Iterator;

    explicit ReadOnlyHashTable(
        HashTable& hashTable,
        boost::optional<RecordSerializer> recordSerializer = boost::none)
        : m_hashTable{ hashTable }
        , m_recordSerializer{
            recordSerializer
            ? *recordSerializer
            : RecordSerializer{
                m_hashTable.m_setting.m_fixedKeySize,
                m_hashTable.m_setting.m_fixedValueSize } }
    {}

    virtual bool Get(const Key& key, Value& value) const override
    {
        const auto bucketInfo = GetBucketInfo(key);
        const auto* entry = &m_hashTable.m_buckets[bucketInfo.first];

        while (entry != nullptr)
        {
            for (std::uint8_t i = 0; i < HashTable::Entry::c_numDataPerEntry; ++i)
            {
                if (bucketInfo.second == entry->m_tags[i])
                {
                    // There could be a race condition where m_dataList[i] is updated during access.
                    // Therefore, load it once and save it (it's safe to store it b/c the memory
                    // will not be deleted until ref count becomes 0).
                    const auto data = entry->m_dataList[i].Load(std::memory_order_acquire);

                    if (data != nullptr)
                    {
                        const auto record = m_recordSerializer.Deserialize(*data);
                        if (record.m_key == key)
                        {
                            value = record.m_value;
                            return true;
                        }
                    }
                }
            }

            entry = entry->m_next.Load(std::memory_order_acquire);
        }

        return false;
    }

    virtual IIteratorPtr GetIterator() const override
    {
        return std::make_unique<Iterator>(m_hashTable, m_recordSerializer);
    }

    virtual const HashTablePerfData& GetPerfData() const override
    {
        // Synchronizes with any std::memory_order_release if there exists, so that
        // HashTablePerfData has the latest values at the moment when GetPerfData() is called.
        std::atomic_thread_fence(std::memory_order_acquire);
        return m_hashTable.m_perfData;
    }

    ReadOnlyHashTable(const ReadOnlyHashTable&) = delete;
    ReadOnlyHashTable& operator=(const ReadOnlyHashTable&) = delete;

protected:
    // GetBucketInfo returns a pair, where the first is the index to the bucket
    // and the second is the tag value for the given key.
    // In this hash table, we treat tag value of 0 as empty (see WritableHashTable::Remove()),
    // so in the worst case scenario, where an entry has an empty data list and the tag
    // value returned for the key is 0, the look up cost is up to 6 checks. We can do something
    // smarter by using the unused two bytes per Entry, but since an Entry object fits into
    // CPU cache, the extra overhead should be minimal.
    std::pair<std::uint32_t, std::uint8_t> GetBucketInfo(const Key& key) const
    {
        std::array<std::uint64_t, 2> hash;
        MurmurHash3_x64_128(key.m_data, key.m_size, 0U, hash.data());

        return {
            static_cast<std::uint32_t>(hash[0] % m_hashTable.m_buckets.size()),
            static_cast<std::uint8_t>(hash[1]) };
    }

    HashTable& m_hashTable;

    RecordSerializer m_recordSerializer;
};


// ReadOnlyHashTable::Iterator class implements IIterator interface and provides
// read-only iterator for the ReadOnlyHashTable.
template <typename Allocator>
class ReadOnlyHashTable<Allocator>::Iterator : public IIterator
{
public:
    Iterator(
        const HashTable& hashTable,
        const RecordSerializer& recordDeserializer)
        : m_hashTable{ hashTable }
        , m_recordSerializer{ recordDeserializer }
        , m_currentBucketIndex{ -1 }
        , m_currentRecordIndex{ 0U }
        , m_currentEntry{ nullptr }
    {}

    Iterator(Iterator&& iterator)
        : m_hashTable{ std::move(iterator.m_hashTable) }
        , m_recordSerializer{ std::move(iterator.recordDeserializer) }
        , m_currentBucketIndex{ std::move(iterator.m_currentBucketIndex) }
        , m_currentRecordIndex{ std::move(iterator.m_currentRecordIndex) }
        , m_currentEntry{ std::move(iterator.m_currentEntry) }
    {}

    void Reset() override
    {
        m_currentBucketIndex = -1;
        m_currentRecordIndex = 0U;
        m_currentEntry = nullptr;
    }

    bool MoveNext() override
    {
        if (IsEnd())
        {
            return false;
        }

        if (m_currentEntry != nullptr)
        {
            MoveToNextData();
        }

        assert(m_currentRecordIndex < HashTable::Entry::c_numDataPerEntry);

        while ((m_currentEntry == nullptr)
            || (m_currentRecord = m_currentEntry->m_dataList[m_currentRecordIndex].Load()) == nullptr)
        {
            if (m_currentEntry == nullptr)
            {
                ++m_currentBucketIndex;
                m_currentRecordIndex = 0U;

                if (IsEnd())
                {
                    return false;
                }

                m_currentEntry = &m_hashTable.m_buckets[m_currentBucketIndex];
            }
            else
            {
                MoveToNextData();
            }
        }

        assert(m_currentEntry != nullptr);
        assert(m_currentRecord != nullptr);

        return true;
    }

    Key GetKey() const override
    {
        if (!IsValid())
        {
            throw RuntimeException("HashTableIterator is not correctly used.");
        }

        return m_recordSerializer.Deserialize(*m_currentRecord).m_key;
    }

    Value GetValue() const override
    {
        if (!IsValid())
        {
            throw RuntimeException("HashTableIterator is not correctly used.");
        }

        return m_recordSerializer.Deserialize(*m_currentRecord).m_value;
    }

    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

private:
    bool IsValid() const
    {
        return !IsEnd()
            && (m_currentEntry != nullptr)
            && (m_currentRecord != nullptr);
    }

    bool IsEnd() const
    {
        return m_currentBucketIndex == static_cast<std::int64_t>(m_hashTable.m_buckets.size());
    }

    void MoveToNextData()
    {
        if (++m_currentRecordIndex >= HashTable::Entry::c_numDataPerEntry)
        {
            m_currentRecordIndex = 0U;
            m_currentEntry = m_currentEntry->m_next.Load();
        }
    }

    const HashTable& m_hashTable;
    const RecordSerializer& m_recordSerializer;

    std::int64_t m_currentBucketIndex;
    std::uint8_t m_currentRecordIndex;

    const typename HashTable::Entry* m_currentEntry;
    const RecordBuffer* m_currentRecord;
};


// The following warning is from the virtual inheritance and safe to disable in this case.
// https://msdn.microsoft.com/en-us/library/6b3sy7ae.aspx
#pragma warning(push)
#pragma warning(disable:4250)

// WritableHashTable class implements IWritableHashTable interface and also provides
// the read only access (Get()) to the hash table.
// Note the virtual inheritance on ReadOnlyHashTable<Allocator> so that any derived class
// can have only one ReadOnlyHashTable base class instance.
template <typename Allocator>
class WritableHashTable
    : public virtual ReadOnlyHashTable<Allocator>
    , public IWritableHashTable
{
public:
    WritableHashTable(
        HashTable& hashTable,
        IEpochActionManager& epochManager)
        : ReadOnlyHashTable(hashTable)
        , m_epochManager{ epochManager }
    {}

    virtual void Add(const Key& key, const Value& value) override
    {
        Add(CreateRecordBuffer(key, value));
    }

    virtual bool Remove(const Key& key) override
    {
        const auto bucketInfo = GetBucketInfo(key);

        auto* entry = &m_hashTable.m_buckets[bucketInfo.first];

        HashTable::Lock lock{ m_hashTable.GetMutex(bucketInfo.first) };

        // Note that similar to Add(), the following block is performed inside a critical section,
        // therefore, it is safe to do "Load"s with memory_order_relaxed.
        while (entry != nullptr)
        {
            for (std::uint8_t i = 0; i < HashTable::Entry::c_numDataPerEntry; ++i)
            {
                if (bucketInfo.second == entry->m_tags[i])
                {
                    const auto data = entry->m_dataList[i].Load(std::memory_order_relaxed);

                    if (data != nullptr)
                    {
                        const auto record = m_recordSerializer.Deserialize(*data);
                        if (record.m_key == key)
                        {
                            Remove(*entry, i);
                            return true;
                        }
                    }
                }
            }

            entry = entry->m_next.Load(std::memory_order_relaxed);
        }

        return false;
    }

    virtual ISerializerPtr GetSerializer() const override
    {
        return std::make_unique<WritableHashTable::Serializer>(m_hashTable);
    }

protected:
    void Add(RecordBuffer* recordToAdd)
    {
        assert(recordToAdd != nullptr);

        const auto newRecord = m_recordSerializer.Deserialize(*recordToAdd);
        const auto& newKey = newRecord.m_key;
        const auto& newValue = newRecord.m_value;

        Stat stat{ newKey.m_size, newValue.m_size };

        const auto bucketInfo = GetBucketInfo(newKey);

        auto* curEntry = &m_hashTable.m_buckets[bucketInfo.first];

        HashTable::Entry* entryToUpdate = nullptr;
        std::uint8_t curDataIndex = 0U;

        HashTable::UniqueLock lock{ m_hashTable.GetMutex(bucketInfo.first) };

        // Note that the following block is performed inside a critical section, therefore,
        // it is safe to do "Load"s with memory_order_relaxed.
        while (curEntry != nullptr)
        {
            ++stat.m_chainIndex;

            for (std::uint8_t i = 0; i < HashTable::Entry::c_numDataPerEntry; ++i)
            {
                const auto data = curEntry->m_dataList[i].Load(std::memory_order_relaxed);

                if (data == nullptr)
                {
                    if (entryToUpdate == nullptr)
                    {
                        // Found an entry with no data set, but still need to go through the end of
                        // the list to see if an entry with the given key exists.
                        entryToUpdate = curEntry;
                        curDataIndex = i;
                    }
                }
                else if (curEntry->m_tags[i] == bucketInfo.second)
                {
                    const auto oldRecord = m_recordSerializer.Deserialize(*data);
                    if (newKey == oldRecord.m_key)
                    {
                        // Will overwrite this entry data.
                        entryToUpdate = curEntry;
                        curDataIndex = i;
                        stat.m_oldValueSize = oldRecord.m_value.m_size;
                        break;
                    }
                }
            }

            // Found the entry data to replaces.
            if (stat.m_oldValueSize != 0U)
            {
                break;
            }

            // Check if this is the end of the chaining. If so, create a new entry if we haven't found
            // any entry to update along the way.
            if (entryToUpdate == nullptr && curEntry->m_next.Load(std::memory_order_relaxed) == nullptr)
            {
                curEntry->m_next.Store(
                    new (Detail::to_raw_pointer(
                        m_hashTable.GetAllocator<HashTable::Entry>().allocate(1U)))
                    HashTable::Entry(),
                    std::memory_order_release);

                stat.m_isNewEntryAdded = true;
            }

            curEntry = curEntry->m_next.Load(std::memory_order_relaxed);
        }

        assert(entryToUpdate != nullptr);

        auto recordToDelete = UpdateRecord(*entryToUpdate, curDataIndex, recordToAdd, bucketInfo.second);

        lock.unlock();

        UpdatePerfDataForAdd(stat);

        ReleaseRecord(recordToDelete);
    }

    // The chainIndex is the 1-based index for the given entry in the chained bucket list.
    // It is assumed that this function is called under a lock.
    void Remove(typename HashTable::Entry& entry, std::uint8_t index)
    {
        auto recordToDelete = UpdateRecord(entry, index, nullptr, 0U);

        assert(recordToDelete != nullptr);

        const auto record = m_recordSerializer.Deserialize(*recordToDelete);

        UpdatePerfDataForRemove(
            Stat{
                record.m_key.m_size,
                record.m_value.m_size,
                0U
            });

        ReleaseRecord(recordToDelete);
    }

private:
    struct Stat;

    class Serializer;

    RecordBuffer* CreateRecordBuffer(const Key& key, const Value& value)
    {
        const auto bufferSize = m_recordSerializer.CalculateBufferSize(key, value);
        auto buffer = Detail::to_raw_pointer(
            m_hashTable.GetAllocator<std::uint8_t>().allocate(bufferSize));
            
        return m_recordSerializer.Serialize(key, value, buffer, bufferSize);
    }

    RecordBuffer* UpdateRecord(typename HashTable::Entry& entry, std::uint8_t index, RecordBuffer* newRecord, std::uint8_t newTag)
    {
        // This function should be called under a lock, so calling with memory_order_relaxed for Load() is safe.
        auto& recordHolder = entry.m_dataList[index];
        auto oldRecord = recordHolder.Load(std::memory_order_relaxed);

        recordHolder.Store(newRecord, std::memory_order_release);
        entry.m_tags[index] = newTag;

        return oldRecord;
    }

    void ReleaseRecord(RecordBuffer* record)
    {
        if (record == nullptr)
        {
            return;
        }

        m_epochManager.RegisterAction(
            [this, record]()
        {
            record->~RecordBuffer();
            m_hashTable.GetAllocator<RecordBuffer>().deallocate(record, 1U);
        });
    }

    void UpdatePerfDataForAdd(const Stat& stat)
    {
        auto& perfData = m_hashTable.m_perfData;

        if (stat.m_oldValueSize != 0U)
        {
            // Updating the existing record. Therefore, no change in the key size.
            perfData.Add(HashTablePerfCounter::TotalValueSize,
                static_cast<HashTablePerfData::TValue>(stat.m_valueSize) - stat.m_oldValueSize);
        }
        else
        {
            // We are adding a new data instead of replacing.
            perfData.Add(HashTablePerfCounter::TotalKeySize, stat.m_keySize);
            perfData.Add(HashTablePerfCounter::TotalValueSize, stat.m_valueSize);
            perfData.Add(HashTablePerfCounter::TotalIndexSize,
                // Record overhead.
                m_recordSerializer.CalculateRecordOverhead()
                // Entry overhead if created.
                + (stat.m_isNewEntryAdded ? sizeof(HashTable::Entry) : 0U));

            perfData.Min(HashTablePerfCounter::MinKeySize, stat.m_keySize);
            perfData.Max(HashTablePerfCounter::MaxKeySize, stat.m_keySize);

            perfData.Increment(HashTablePerfCounter::RecordsCount);

            if (stat.m_isNewEntryAdded)
            {
                perfData.Increment(HashTablePerfCounter::ChainingEntriesCount);

                if (stat.m_chainIndex > 1U)
                {
                    perfData.Max(HashTablePerfCounter::MaxBucketChainLength, stat.m_chainIndex);
                }
            }
        }

        perfData.Min(HashTablePerfCounter::MinValueSize, stat.m_valueSize);
        perfData.Max(HashTablePerfCounter::MaxValueSize, stat.m_valueSize);
    }

    void UpdatePerfDataForRemove(const Stat& stat)
    {
        auto& perfData = m_hashTable.m_perfData;

        perfData.Decrement(HashTablePerfCounter::RecordsCount);
        perfData.Subtract(HashTablePerfCounter::TotalKeySize, stat.m_keySize);
        perfData.Subtract(HashTablePerfCounter::TotalValueSize, stat.m_valueSize);
        perfData.Subtract(HashTablePerfCounter::TotalIndexSize, m_recordSerializer.CalculateRecordOverhead());
    }

    IEpochActionManager& m_epochManager;
};

#pragma warning(pop)


// WritableHashTable::Stat struct encapsulates stats for Add()/Remove().
template <typename Allocator>
struct WritableHashTable<Allocator>::Stat
{
    using KeySize = Key::size_type;
    using ValueSize = Value::size_type;

    explicit Stat(
        KeySize keySize = 0U,
        ValueSize valueSize = 0U,
        ValueSize oldValueSize = 0U,
        std::uint32_t chainIndex = 0U,
        bool isNewEntryAdded = false)
        : m_keySize{ keySize }
        , m_valueSize{ valueSize }
        , m_oldValueSize{ oldValueSize }
        , m_chainIndex{ chainIndex }
        , m_isNewEntryAdded{ isNewEntryAdded }
    {}

    KeySize m_keySize;
    ValueSize m_valueSize;
    ValueSize m_oldValueSize;
    std::uint32_t m_chainIndex;
    bool m_isNewEntryAdded;
};


// WritableHashTable::Serializer class that implements ISerializer, which provides
// the functionality to serialize the WritableHashTable.
template <typename Allocator>
class WritableHashTable<Allocator>::Serializer : public IWritableHashTable::ISerializer
{
public:
    explicit Serializer(HashTable& hashTable)
        : m_hashTable{ hashTable }
    {}

    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;

    void Serialize(
        std::ostream& stream,
        const Utils::Properties& /* properties */) override
    {
        ReadWrite::Serializer<HashTable>{}.Serialize(m_hashTable, stream);
    }

private:
    HashTable& m_hashTable;
};

} // namespace ReadWrite
} // namespace HashTable
} // namespace L4