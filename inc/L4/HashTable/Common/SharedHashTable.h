#pragma once

#include <cstdint>
#include <mutex>

#include "HashTable/IHashTable.h"
#include "Interprocess/Container/Vector.h"
#include "Log/PerfCounter.h"
#include "Utils/AtomicOffsetPtr.h"
#include "Utils/Exception.h"
#include "Utils/Lock.h"

namespace L4
{
namespace HashTable
{

// SharedHashTable struct represents the hash table structure.
template <typename TData, typename TAllocator>
struct SharedHashTable
{
    using Data = TData;
    using Allocator = TAllocator;

    // HashTable::Entry struct represents an entry in the chained bucket list.
    // Entry layout is as follows:
    //
    // | tag1  | tag2  | tag3  | tag4  | tag5  | tag6  | tag7  | tag 8  | 1
    // | tag9  | tag10 | tag11 | tag12 | tag13 | tag14 | tag15 | tag 16 | 2
    // | Data1 pointer                                                  | 3
    // | Data2 pointer                                                  | 4
    // | Data3 pointer                                                  | 5
    // | Data4 pointer                                                  | 6
    // | Data5 pointer                                                  | 7
    // | Data6 pointer                                                  | 8
    // | Data7 pointer                                                  | 9
    // | Data8 pointer                                                  | 10
    // | Data9 pointer                                                  | 11
    // | Data10 pointer                                                 | 12
    // | Data11 pointer                                                 | 13
    // | Data12 pointer                                                 | 14
    // | Data13 pointer                                                 | 15
    // | Data14 pointer                                                 | 16
    // | Data15 pointer                                                 | 17
    // | Data16 pointer                                                 | 18
    // | Entry pointer to the next Entry                                | 19
    // <----------------------8 bytes ---------------------------------->
    // , where tag1 is a tag for Data1, tag2 for Data2, and so on. A tag value can be looked up
    // first before going to the corresponding Data for a quick check.
    // Also note that a byte read is atomic in modern processors so that tag is just
    // std::uint8_t instead of being atomic. Even in the case where the tag value read is a garbage ,
    // this is acceptable because of the followings:
    //    1) if the garbage value was a hit where it should have been a miss: the actual key comparison will fail,
    //    2) if the garbage value was a miss where it should have been a hit: the key value must
    //       have been changed since the tag was changed, so it will be looked up correctly
    //       after the tag value written is visible correctly. Note that we don't need to guarantee the timing of
    //       writing and reading (meaning the value written should be visible to the reader right away).
    //
    // Note about the CPU cache. In previous implementation, the Entry was 64 bytes to fit in the CPU cache.
    // However, this resulted in lots of wasted space. For example, when the ratio of the number of expected records
    // to the number of buckets was 2:1, only 85% buckets were occupied. After experiments, if you have 10:1 ratio,
    // you will have 99.98% utilization of buckets. This required having more data per Entry, and the ideal number
    // (after experiments) turned out to be 16 records per Entry. Also, because of how CPU fetches contiguous memory,
    // this didn't have any impact on micro-benchmarking.
    struct Entry
    {
        Entry() = default;

        // Releases deallocates all the memories of the chained entries including
        // the data list in the current Entry.
        void Release(Allocator allocator)
        {
            auto dataDeleter = [allocator](auto& data)
            {
                auto dataToDelete = data.Load();
                if (dataToDelete != nullptr)
                {
                    dataToDelete->~Data();
                    Allocator::rebind<Data>::other(allocator).deallocate(dataToDelete, 1U);
                }
            };
        
            // Delete all the chained entries, not including itself.
            auto curEntry = m_next.Load();
        
            while (curEntry != nullptr)
            {
                auto entryToDelete = curEntry;
        
                // Copy m_next for the next iteration.
                curEntry = entryToDelete->m_next.Load();

                // Delete all the data within this entry.
                for (auto& data : entryToDelete->m_dataList)
                {
                    dataDeleter(data);
                }
        
                // Clean the current entry itself.
                entryToDelete->~Entry();
                Allocator::rebind<Entry>::other(allocator).deallocate(entryToDelete, 1U);
            }
        
            // Delete all the data from the head of chained entries.
            for (auto& data : m_dataList)
            {
                dataDeleter(data);
            }
        }

        static constexpr std::uint8_t c_numDataPerEntry = 16U;

        std::array<std::uint8_t, c_numDataPerEntry> m_tags{ 0U };

        std::array<Utils::AtomicOffsetPtr<Data>, c_numDataPerEntry> m_dataList{};
        
        Utils::AtomicOffsetPtr<Entry> m_next{};
    };

    static_assert(sizeof(Entry) == 152, "Entry should be 152 bytes.");

    struct Setting
    {
        using KeySize = IReadOnlyHashTable::Key::size_type;
        using ValueSize = IReadOnlyHashTable::Value::size_type;

        Setting() = default;

        explicit Setting(
            std::uint32_t numBuckets,
            std::uint32_t numBucketsPerMutex = 1U,
            KeySize fixedKeySize = 0U,
            ValueSize fixedValueSize = 0U)
            : m_numBuckets{ numBuckets }
            , m_numBucketsPerMutex{ numBucketsPerMutex }
            , m_fixedKeySize{ fixedKeySize }
            , m_fixedValueSize{ fixedValueSize }
        {}

        std::uint32_t m_numBuckets = 1U;
        std::uint32_t m_numBucketsPerMutex = 1U;
        KeySize m_fixedKeySize = 0U;
        ValueSize m_fixedValueSize = 0U;
    };

    SharedHashTable::SharedHashTable(
        const Setting& setting,
        Allocator allocator)
        : m_allocator{ allocator }
        , m_setting{ setting }
        , m_buckets{ setting.m_numBuckets, Allocator::rebind<Entry>::other(m_allocator) }
        , m_mutexes{
            (std::max)(setting.m_numBuckets / (std::max)(setting.m_numBucketsPerMutex, 1U), 1U),
            Allocator::rebind<Mutex>::other(m_allocator) }
        , m_perfData{}
    {
        m_perfData.Set(HashTablePerfCounter::BucketsCount, m_buckets.size());
        m_perfData.Set(
            HashTablePerfCounter::TotalIndexSize,
            (m_buckets.size() * sizeof(Entry))
            + (m_mutexes.size() * sizeof(Mutex))
            + sizeof(SharedHashTable));
    }

    SharedHashTable::~SharedHashTable()
    {
        for (auto& bucket : m_buckets)
        {
            bucket.Release(m_allocator);
        }
    }

    using Mutex = Utils::ReaderWriterLockSlim;
    using Lock = std::lock_guard<Mutex>;
    using UniqueLock = std::unique_lock<Mutex>;

    using Buckets = Interprocess::Container::Vector<Entry, typename Allocator::template rebind<Entry>::other>;
    using Mutexes = Interprocess::Container::Vector<Mutex, typename Allocator::template rebind<Mutex>::other>;

    template <typename T>
    auto GetAllocator() const
    {
        return Allocator::rebind<T>::other(m_allocator);
    }

    Mutex& GetMutex(std::size_t index)
    {
        return m_mutexes[index % m_mutexes.size()];
    }

    Allocator m_allocator;

    const Setting m_setting;

    Buckets m_buckets;

    Mutexes m_mutexes;

    HashTablePerfData m_perfData;

    SharedHashTable(const SharedHashTable&) = delete;
    SharedHashTable& operator=(const SharedHashTable&) = delete;
};

} // namespace HashTable
} // namespace L4
