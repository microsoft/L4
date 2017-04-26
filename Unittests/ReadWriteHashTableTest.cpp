#include <boost/test/unit_test.hpp>
#include "Utils.h"
#include "Mocks.h"
#include "CheckedAllocator.h"
#include "L4/Log/PerfCounter.h"
#include "L4/HashTable/ReadWrite/HashTable.h"

namespace L4
{
namespace UnitTests
{

using namespace HashTable::ReadWrite;

class ReadWriteHashTableTestFixture
{
protected:
    using Allocator = CheckedAllocator<>;
    using HashTable = WritableHashTable<Allocator>::HashTable;

    ReadWriteHashTableTestFixture()
        : m_allocator{}
        , m_epochManager{}
    {}

    Allocator m_allocator;
    MockEpochManager m_epochManager;
};


BOOST_FIXTURE_TEST_SUITE(ReadWriteHashTableTests, ReadWriteHashTableTestFixture)


BOOST_AUTO_TEST_CASE(HashTableTest)
{
    HashTable hashTable{ HashTable::Setting{ 100, 5 }, m_allocator };
    WritableHashTable<Allocator> writableHashTable(hashTable, m_epochManager);
    ReadOnlyHashTable<Allocator> readOnlyHashTable(hashTable);

    const auto& perfData = writableHashTable.GetPerfData();

    {
        // Check empty data.

        std::string keyStr = "hello";
        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());

        IReadOnlyHashTable::Value data;
        BOOST_CHECK(!readOnlyHashTable.Get(key, data));

        const auto c_counterMaxValue = (std::numeric_limits<HashTablePerfData::TValue>::max)();
        Utils::ValidateCounters(
            perfData,
            {
                { HashTablePerfCounter::RecordsCount, 0 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::ChainingEntriesCount, 0 },
                { HashTablePerfCounter::TotalKeySize, 0 },
                { HashTablePerfCounter::TotalValueSize, 0 },
                { HashTablePerfCounter::MinKeySize, c_counterMaxValue },
                { HashTablePerfCounter::MaxKeySize, 0 },
                { HashTablePerfCounter::MinValueSize, c_counterMaxValue },
                { HashTablePerfCounter::MaxValueSize, 0 }
            });
    }


    {
        // First record added.
        std::string keyStr = "hello";
        std::string valStr = "world";

        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valStr.c_str());

        writableHashTable.Add(key, val);

        IReadOnlyHashTable::Value value;
        BOOST_CHECK(readOnlyHashTable.Get(key, value));
        BOOST_CHECK(value.m_size == valStr.size());
        BOOST_CHECK(!memcmp(value.m_data, valStr.c_str(), valStr.size()));

        Utils::ValidateCounters(
            perfData,
            {
                { HashTablePerfCounter::RecordsCount, 1 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::ChainingEntriesCount, 0 },
                { HashTablePerfCounter::TotalKeySize, 5 },
                { HashTablePerfCounter::TotalValueSize, 5 },
                { HashTablePerfCounter::MinKeySize, 5 },
                { HashTablePerfCounter::MaxKeySize, 5 },
                { HashTablePerfCounter::MinValueSize, 5 },
                { HashTablePerfCounter::MaxValueSize, 5 }
            });
    }

    {
        // Second record added.
        std::string keyStr = "hello2";
        std::string valStr = "world2";

        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valStr.c_str());

        writableHashTable.Add(key, val);

        IReadOnlyHashTable::Value value;
        BOOST_CHECK(readOnlyHashTable.Get(key, value));
        BOOST_CHECK(value.m_size == valStr.size());
        BOOST_CHECK(!memcmp(value.m_data, valStr.c_str(), valStr.size()));

        Utils::ValidateCounters(
            perfData,
            {
                { HashTablePerfCounter::RecordsCount, 2 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::ChainingEntriesCount, 0 },
                { HashTablePerfCounter::TotalKeySize, 11 },
                { HashTablePerfCounter::TotalValueSize, 11 },
                { HashTablePerfCounter::MinKeySize, 5 },
                { HashTablePerfCounter::MaxKeySize, 6 },
                { HashTablePerfCounter::MinValueSize, 5 },
                { HashTablePerfCounter::MaxValueSize, 6 }
            });
    }

    {
        // Update the key with value bigger than the existing values.
        std::string keyStr = "hello";
        std::string valStr = "world long string";

        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valStr.c_str());

        writableHashTable.Add(key, val);

        IReadOnlyHashTable::Value value;
        BOOST_CHECK(readOnlyHashTable.Get(key, value));
        BOOST_CHECK(value.m_size == valStr.size());
        BOOST_CHECK(!memcmp(value.m_data, valStr.c_str(), valStr.size()));
        BOOST_CHECK(m_epochManager.m_numRegisterActionsCalled == 1);

        Utils::ValidateCounters(
            perfData,
            {
                { HashTablePerfCounter::RecordsCount, 2 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::ChainingEntriesCount, 0 },
                { HashTablePerfCounter::TotalKeySize, 11 },
                { HashTablePerfCounter::TotalValueSize, 23 },
                { HashTablePerfCounter::MinKeySize, 5 },
                { HashTablePerfCounter::MaxKeySize, 6 },
                { HashTablePerfCounter::MinValueSize, 5 },
                { HashTablePerfCounter::MaxValueSize, 17 }
            });
    }
    
    {
        // Update the key with value smaller than the existing values.
        std::string keyStr = "hello2";
        std::string valStr = "wo";

        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valStr.c_str());

        writableHashTable.Add(key, val);

        IReadOnlyHashTable::Value value;
        BOOST_CHECK(readOnlyHashTable.Get(key, value));
        BOOST_CHECK(value.m_size == valStr.size());
        BOOST_CHECK(!memcmp(value.m_data, valStr.c_str(), valStr.size()));
        BOOST_CHECK(m_epochManager.m_numRegisterActionsCalled == 2);

        Utils::ValidateCounters(
            perfData,
            {
                { HashTablePerfCounter::RecordsCount, 2 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::ChainingEntriesCount, 0 },
                { HashTablePerfCounter::TotalKeySize, 11 },
                { HashTablePerfCounter::TotalValueSize, 19 },
                { HashTablePerfCounter::MinKeySize, 5 },
                { HashTablePerfCounter::MaxKeySize, 6 },
                { HashTablePerfCounter::MinValueSize, 2 },
                { HashTablePerfCounter::MaxValueSize, 17 }
            });
    }

    {
        // Remove the first key.
        std::string keyStr = "hello";
        std::string valStr = "";

        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valStr.c_str());

        BOOST_CHECK(writableHashTable.Remove(key));
        BOOST_CHECK(m_epochManager.m_numRegisterActionsCalled == 3);

        // Note that the Remove() doesn't change Min/Max counters by design.
        Utils::ValidateCounters(
            perfData,
            {
                { HashTablePerfCounter::RecordsCount, 1 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::ChainingEntriesCount, 0 },
                { HashTablePerfCounter::TotalKeySize, 6 },
                { HashTablePerfCounter::TotalValueSize, 2 },
                { HashTablePerfCounter::MinKeySize, 5 },
                { HashTablePerfCounter::MaxKeySize, 6 },
                { HashTablePerfCounter::MinValueSize, 2 },
                { HashTablePerfCounter::MaxValueSize, 17 }
            });

        // Remove the second key.
        keyStr = "hello2";
        key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());

        BOOST_CHECK(writableHashTable.Remove(key));
        BOOST_CHECK(m_epochManager.m_numRegisterActionsCalled == 4);

        Utils::ValidateCounters(
            perfData,
            {
                { HashTablePerfCounter::RecordsCount, 0 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::ChainingEntriesCount, 0 },
                { HashTablePerfCounter::TotalKeySize, 0 },
                { HashTablePerfCounter::TotalValueSize, 0 },
                { HashTablePerfCounter::MinKeySize, 5 },
                { HashTablePerfCounter::MaxKeySize, 6 },
                { HashTablePerfCounter::MinValueSize, 2 },
                { HashTablePerfCounter::MaxValueSize, 17 }
            });

        // Removing the key that doesn't exist.
        BOOST_CHECK(!writableHashTable.Remove(key));
        Utils::ValidateCounters(
            perfData,
            {
                { HashTablePerfCounter::RecordsCount, 0 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::ChainingEntriesCount, 0 },
                { HashTablePerfCounter::TotalKeySize, 0 },
                { HashTablePerfCounter::TotalValueSize, 0 },
                { HashTablePerfCounter::MinKeySize, 5 },
                { HashTablePerfCounter::MaxKeySize, 6 },
                { HashTablePerfCounter::MinValueSize, 2 },
                { HashTablePerfCounter::MaxValueSize, 17 }
            });
    }
}


BOOST_AUTO_TEST_CASE(HashTableWithOneBucketTest)
{
    Allocator allocator;
    HashTable hashTable{ HashTable::Setting{ 1 }, allocator };
    WritableHashTable<Allocator> writableHashTable(hashTable, m_epochManager);
    ReadOnlyHashTable<Allocator> readOnlyHashTable(hashTable);

    const auto& perfData = writableHashTable.GetPerfData();

    Utils::ValidateCounters(perfData, { { HashTablePerfCounter::ChainingEntriesCount, 0 } });

    const auto initialTotalIndexSize = perfData.Get(HashTablePerfCounter::TotalIndexSize);
    const std::size_t c_dataSetSize = HashTable::Entry::c_numDataPerEntry + 5U;

    std::size_t expectedTotalKeySize = 0U;
    std::size_t expectedTotalValueSize = 0U;

    for (auto i = 0U; i < c_dataSetSize; ++i)
    {
        std::stringstream keyStream;
        keyStream << "key" << i;

        std::stringstream valStream;
        valStream << "value" << i;

        std::string keyStr = keyStream.str();
        std::string valStr = valStream.str();

        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valStr.c_str());

        expectedTotalKeySize += key.m_size;
        expectedTotalValueSize += val.m_size;

        writableHashTable.Add(key, val);

        IReadOnlyHashTable::Value value;
        BOOST_CHECK(readOnlyHashTable.Get(key, value));
        BOOST_CHECK(value.m_size == valStr.size());
        BOOST_CHECK(!memcmp(value.m_data, valStr.c_str(), valStr.size()));
    }

    using L4::HashTable::RecordSerializer;

    // Variable key/value sizes.
    const auto recordOverhead = RecordSerializer{ 0U, 0U }.CalculateRecordOverhead();

    Utils::ValidateCounters(
        perfData,
        {
            { HashTablePerfCounter::RecordsCount, c_dataSetSize },
            { HashTablePerfCounter::BucketsCount, 1 },
            { HashTablePerfCounter::MaxBucketChainLength, 2 },
            { HashTablePerfCounter::ChainingEntriesCount, 1 },
            { HashTablePerfCounter::TotalKeySize, expectedTotalKeySize },
            { HashTablePerfCounter::TotalValueSize, expectedTotalValueSize },
            {
                HashTablePerfCounter::TotalIndexSize,
                initialTotalIndexSize + sizeof(HashTable::Entry) + (c_dataSetSize * recordOverhead)
            }
        });

    // Now replace with new values.
    expectedTotalValueSize = 0U;

    for (auto i = 0U; i < c_dataSetSize; ++i)
    {
        std::stringstream keyStream;
        keyStream << "key" << i;

        std::stringstream valStream;
        valStream << "val" << i;

        std::string keyStr = keyStream.str();
        std::string valStr = valStream.str();

        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valStr.c_str());
        
        expectedTotalValueSize += val.m_size;

        writableHashTable.Add(key, val);

        IReadOnlyHashTable::Value value;
        BOOST_CHECK(readOnlyHashTable.Get(key, value));
        BOOST_CHECK(value.m_size == valStr.size());
        BOOST_CHECK(!memcmp(value.m_data, valStr.c_str(), valStr.size()));
    }

    Utils::ValidateCounters(
        perfData,
        {
            { HashTablePerfCounter::RecordsCount, c_dataSetSize },
            { HashTablePerfCounter::BucketsCount, 1 },
            { HashTablePerfCounter::MaxBucketChainLength, 2 },
            { HashTablePerfCounter::ChainingEntriesCount, 1 },
            { HashTablePerfCounter::TotalKeySize, expectedTotalKeySize },
            { HashTablePerfCounter::TotalValueSize, expectedTotalValueSize },
            {
                HashTablePerfCounter::TotalIndexSize,
                initialTotalIndexSize + sizeof(HashTable::Entry) + (c_dataSetSize * recordOverhead)
            }
        });

    // Now remove all key-value.
    for (auto i = 0U; i < c_dataSetSize; ++i)
    {
        std::stringstream keyStream;
        keyStream << "key" << i;

        std::string keyStr = keyStream.str();
        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());

        BOOST_CHECK(writableHashTable.Remove(key));

        IReadOnlyHashTable::Value value;
        BOOST_CHECK(!readOnlyHashTable.Get(key, value));
    }

    Utils::ValidateCounters(
        perfData,
        {
            { HashTablePerfCounter::RecordsCount, 0 },
            { HashTablePerfCounter::BucketsCount, 1 },
            { HashTablePerfCounter::MaxBucketChainLength, 2 },
            { HashTablePerfCounter::ChainingEntriesCount, 1 },
            { HashTablePerfCounter::TotalKeySize, 0 },
            { HashTablePerfCounter::TotalValueSize, 0 },
            {
                HashTablePerfCounter::TotalIndexSize,
                initialTotalIndexSize + sizeof(HashTable::Entry)
            }
        });

    // Try to add back to the same bucket (reusing existing entries)
    expectedTotalKeySize = 0U;
    expectedTotalValueSize = 0U;

    for (auto i = 0U; i < c_dataSetSize; ++i)
    {
        std::stringstream keyStream;
        keyStream << "key" << i;

        std::stringstream valStream;
        valStream << "value" << i;

        std::string keyStr = keyStream.str();
        std::string valStr = valStream.str();

        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valStr.c_str());

        expectedTotalKeySize += key.m_size;
        expectedTotalValueSize += val.m_size;

        writableHashTable.Add(key, val);

        IReadOnlyHashTable::Value value;
        BOOST_CHECK(readOnlyHashTable.Get(key, value));
        BOOST_CHECK(value.m_size == valStr.size());
        BOOST_CHECK(!memcmp(value.m_data, valStr.c_str(), valStr.size()));
    }

    Utils::ValidateCounters(
        perfData,
        {
            { HashTablePerfCounter::RecordsCount, c_dataSetSize },
            { HashTablePerfCounter::BucketsCount, 1 },
            { HashTablePerfCounter::MaxBucketChainLength, 2 },
            { HashTablePerfCounter::ChainingEntriesCount, 1 },
            { HashTablePerfCounter::TotalKeySize, expectedTotalKeySize },
            { HashTablePerfCounter::TotalValueSize, expectedTotalValueSize },
            {
                HashTablePerfCounter::TotalIndexSize,
                initialTotalIndexSize + sizeof(HashTable::Entry) + (c_dataSetSize * recordOverhead)
            }
        });
}


BOOST_AUTO_TEST_CASE(AddRemoveSameKeyTest)
{
    HashTable hashTable{ HashTable::Setting{ 100, 5 }, m_allocator };
    WritableHashTable<Allocator> writableHashTable(hashTable, m_epochManager);
    ReadOnlyHashTable<Allocator> readOnlyHashTable(hashTable);

    // Add two key/value pairs.
    auto key1 = Utils::ConvertFromString<IReadOnlyHashTable::Key>("key1");
    auto val1 = Utils::ConvertFromString<IReadOnlyHashTable::Value>("val1");
    writableHashTable.Add(key1, val1);

    IReadOnlyHashTable::Value valueRetrieved;
    BOOST_CHECK(readOnlyHashTable.Get(key1, valueRetrieved));
    BOOST_CHECK(valueRetrieved.m_size == val1.m_size);
    BOOST_CHECK(!memcmp(valueRetrieved.m_data, val1.m_data, val1.m_size));

    auto key2 = Utils::ConvertFromString<IReadOnlyHashTable::Key>("key2");
    auto val2 = Utils::ConvertFromString<IReadOnlyHashTable::Value>("val2");
    writableHashTable.Add(key2, val2);

    BOOST_CHECK(readOnlyHashTable.Get(key2, valueRetrieved));
    BOOST_CHECK(valueRetrieved.m_size == val2.m_size);
    BOOST_CHECK(!memcmp(valueRetrieved.m_data, val2.m_data, val2.m_size));

    const auto& perfData = writableHashTable.GetPerfData();

    // Now remove the first record with key = "key1", which is at the head of the chain.
    BOOST_CHECK(writableHashTable.Remove(key1));
    BOOST_CHECK(!readOnlyHashTable.Get(key1, valueRetrieved));
    Utils::ValidateCounter(perfData, HashTablePerfCounter::RecordsCount, 1);

    // Now try update the record with key = "key2". This should correctly update the existing record
    // instead of using the empty slot created by removing the record with key = "key1".
    auto newVal2 = Utils::ConvertFromString<IReadOnlyHashTable::Value>("newVal2");
    writableHashTable.Add(key2, newVal2);
    
    BOOST_CHECK(readOnlyHashTable.Get(key2, valueRetrieved));
    BOOST_CHECK(valueRetrieved.m_size == newVal2.m_size);
    BOOST_CHECK(!memcmp(valueRetrieved.m_data, newVal2.m_data, newVal2.m_size));
    Utils::ValidateCounter(perfData, HashTablePerfCounter::RecordsCount, 1);

    // Remove the record with key = "key2".
    BOOST_CHECK(writableHashTable.Remove(key2));
    BOOST_CHECK(!writableHashTable.Remove(key2));
    Utils::ValidateCounter(perfData, HashTablePerfCounter::RecordsCount, 0);
}


BOOST_AUTO_TEST_CASE(FixedKeyValueHashTableTest)
{
    // Fixed 4 byte keys and 6 byte values.
    std::vector<HashTable::Setting> settings =
    {
        HashTable::Setting{ 100, 200, 4, 0 },
        HashTable::Setting{ 100, 200, 0, 6 },
        HashTable::Setting{ 100, 200, 4, 6 }
    };

    for (const auto& setting : settings)
    {
        HashTable hashTable{ setting, m_allocator };
        WritableHashTable<Allocator> writableHashTable(hashTable, m_epochManager);
        ReadOnlyHashTable<Allocator> readOnlyHashTable(hashTable);

        constexpr std::uint8_t c_numRecords = 10;

        for (std::uint8_t i = 0; i < c_numRecords; ++i)
        {
            const std::string keyStr = "key" + std::to_string(i);
            const std::string valueStr = "value" + std::to_string(i);

            writableHashTable.Add(
                Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str()),
                Utils::ConvertFromString<IReadOnlyHashTable::Value>(valueStr.c_str()));
        }

        Utils::ValidateCounters(
            writableHashTable.GetPerfData(),
            {
                { HashTablePerfCounter::RecordsCount, 10 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::TotalKeySize, 40 },
                { HashTablePerfCounter::TotalValueSize, 60 },
                { HashTablePerfCounter::MinKeySize, 4 },
                { HashTablePerfCounter::MaxKeySize, 4 },
                { HashTablePerfCounter::MinValueSize, 6 },
                { HashTablePerfCounter::MaxValueSize, 6 }
            });

        for (std::uint8_t i = 0; i < c_numRecords; ++i)
        {
            const std::string keyStr = "key" + std::to_string(i);
            const std::string valueStr = "value" + std::to_string(i);
            const auto expectedValue = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valueStr.c_str());

            IReadOnlyHashTable::Value actualValue;
            BOOST_CHECK(readOnlyHashTable.Get(
                Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str()),
                actualValue));
            BOOST_CHECK(expectedValue == actualValue);
        }

        for (std::uint8_t i = 0; i < c_numRecords; ++i)
        {
            const std::string keyStr = "key" + std::to_string(i);
            writableHashTable.Remove(
                Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str()));
        }

        Utils::ValidateCounters(
            writableHashTable.GetPerfData(),
            {
                { HashTablePerfCounter::RecordsCount, 0 },
                { HashTablePerfCounter::BucketsCount, 100 },
                { HashTablePerfCounter::TotalKeySize, 0 },
                { HashTablePerfCounter::TotalValueSize, 0 }
            });
    }
}


BOOST_AUTO_TEST_CASE(HashTableIteratorTest)
{
    Allocator allocator;
    constexpr std::uint32_t c_numBuckets = 10;
    HashTable hashTable{ HashTable::Setting{ c_numBuckets }, allocator };
    WritableHashTable<Allocator> writableHashTable(hashTable, m_epochManager);

    {
        // Empty data set, thus iterator cannot move.
        auto iter = writableHashTable.GetIterator();
        BOOST_CHECK(!iter->MoveNext());

        CHECK_EXCEPTION_THROWN_WITH_MESSAGE(
            iter->GetKey(),
            "HashTableIterator is not correctly used.");

        CHECK_EXCEPTION_THROWN_WITH_MESSAGE(
            iter->GetValue(),
            "HashTableIterator is not correctly used.");
    }

    using Buffer = std::vector<std::uint8_t>;
    using BufferMap = std::map<Buffer, Buffer>;

    BufferMap keyValueMap;

    // The number of records should be such that it will create chained entries
    // for at least one bucket. So it should be greater than HashTable::Entry::c_numDataPerEntry * number of buckets.
    constexpr std::uint32_t c_numRecords = (HashTable::Entry::c_numDataPerEntry * c_numBuckets) + 1;

    for (auto i = 0U; i < c_numRecords; ++i)
    {
        std::stringstream keyStream;
        keyStream << "key" << i;

        std::stringstream valStream;
        valStream << "value" << i;

        std::string keyStr = keyStream.str();
        std::string valStr = valStream.str();

        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(valStr.c_str());

        writableHashTable.Add(key, val);

        keyValueMap[Buffer(key.m_data, key.m_data + key.m_size)] = Buffer(val.m_data, val.m_data + val.m_size);
    }

    BOOST_REQUIRE(writableHashTable.GetPerfData().Get(HashTablePerfCounter::MaxBucketChainLength) >= 2);
    BOOST_CHECK_EQUAL(keyValueMap.size(), c_numRecords);

    {
        BufferMap keyValueMapFromIterator;

        // Validate the data using the iterator.
        auto iter = writableHashTable.GetIterator();
        for (auto i = 0U; i < c_numRecords; ++i)
        {
            BOOST_CHECK(iter->MoveNext());

            const auto& key = iter->GetKey();
            const auto& val = iter->GetValue();

            keyValueMapFromIterator[Buffer(key.m_data, key.m_data + key.m_size)] = Buffer(val.m_data, val.m_data + val.m_size);
        }
        BOOST_CHECK(!iter->MoveNext());
        BOOST_CHECK(keyValueMap == keyValueMapFromIterator);

        // Reset should move the iterator to the beginning.
        iter->Reset();
        for (auto i = 0U; i < c_numRecords; ++i)
        {
            BOOST_CHECK(iter->MoveNext());
        }
        BOOST_CHECK(!iter->MoveNext());
    }

    // Remove half of the key.
    for (auto i = 0U; i < c_numRecords; ++i)
    {
        if (i % 2 == 0U)
        {
            std::stringstream keyStream;
            keyStream << "key" << i;

            std::string keyStr = keyStream.str();
            auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(keyStr.c_str());

            BOOST_CHECK(writableHashTable.Remove(key));

            keyValueMap.erase(Buffer(key.m_data, key.m_data + key.m_size));
        }
    }

    BOOST_CHECK_EQUAL(keyValueMap.size(), c_numRecords / 2U);

    // Validate only the existing keys are iterated.
    {
        BufferMap keyValueMapFromIterator;
        auto iter = writableHashTable.GetIterator();
        for (auto i = 0U; i < c_numRecords / 2U; ++i)
        {
            BOOST_CHECK(iter->MoveNext());

            const auto& key = iter->GetKey();
            const auto& val = iter->GetValue();

            keyValueMapFromIterator[Buffer(key.m_data, key.m_data + key.m_size)] = 
                Buffer(val.m_data, val.m_data + val.m_size);
        }
        BOOST_CHECK(!iter->MoveNext());
        BOOST_CHECK(keyValueMap == keyValueMapFromIterator);
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace UnitTests
} // namespace L4
