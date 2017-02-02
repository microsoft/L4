#include "stdafx.h"
#include "Utils.h"
#include "Mocks.h"
#include "L4/HashTable/ReadWrite/HashTable.h"
#include "L4/HashTable/ReadWrite/Serializer.h"
#include "L4/Log/PerfCounter.h"
#include <string>
#include <vector>

namespace L4
{
namespace UnitTests
{

class LocalMemory
{
public:
    template <typename T = void>
    using Allocator = typename std::allocator<T>;

    template <typename T>
    using Deleter = typename std::default_delete<T>;

    template <typename T>
    using UniquePtr = std::unique_ptr<T>;

    LocalMemory() = default;

    template <typename T, typename... Args>
    auto MakeUnique(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename T = void>
    auto GetAllocator()
    {
        return Allocator<T>();
    }

    template <typename T>
    auto GetDeleter()
    {
        return Deleter<T>();
    }

    LocalMemory(const LocalMemory&) = delete;
    LocalMemory& operator=(const LocalMemory&) = delete;
};

using namespace HashTable::ReadWrite;

BOOST_AUTO_TEST_SUITE(HashTableSerializerTests)

using KeyValuePair = std::pair<std::string, std::string>;
using KeyValuePairs = std::vector<KeyValuePair>;
using Memory = LocalMemory;
using Allocator = typename Memory:: template Allocator<>;
using HashTable = WritableHashTable<Allocator>::HashTable;

template <typename Serializer, typename Deserializer>
void ValidateSerializer(
    const Serializer& serializer,
    const Deserializer& deserializer,
    std::uint8_t serializerVersion,
    const KeyValuePairs& keyValuePairs,
    const Utils::ExpectedCounterValues& expectedCounterValuesAfterLoad,
    const Utils::ExpectedCounterValues& expectedCounterValuesAfterSerialization,
    const Utils::ExpectedCounterValues& expectedCounterValuesAfterDeserialization)
{
    LocalMemory memory;
    MockEpochManager epochManager;

    auto hashTableHolder{
        memory.MakeUnique<HashTable>(
            HashTable::Setting{ 5 }, memory.GetAllocator()) };
    BOOST_CHECK(hashTableHolder != nullptr);

    WritableHashTable<Allocator> writableHashTable(*hashTableHolder, epochManager);

    // Insert the given key/value pairs to the hash table.
    for (const auto& pair : keyValuePairs)
    {
        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(pair.first.c_str());
        auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(pair.second.c_str());

        writableHashTable.Add(key, val);
    }

    const auto& perfData = writableHashTable.GetPerfData();

    Utils::ValidateCounters(perfData, expectedCounterValuesAfterLoad);

    // Now write the hash table to the stream.
    MockStreamWriter writer;
    BOOST_CHECK(!writer.IsValid());
    serializer.Serialize(*hashTableHolder, writer);
    BOOST_CHECK(writer.IsValid());
    Utils::ValidateCounters(perfData, expectedCounterValuesAfterSerialization);

    // Read in the hash table from the stream and validate it.
    MockStreamReader reader(writer.GetStreamBuffer());

    // version == 0 means that it's run through the HashTableSerializer, thus the following can be skipped.
    if (serializerVersion != 0)
    {
        std::uint8_t actualSerializerVersion = 0;
        reader.Begin();
        reader.Read(&actualSerializerVersion, sizeof(actualSerializerVersion));
        BOOST_CHECK(actualSerializerVersion == serializerVersion);
    }
    else
    {
        BOOST_REQUIRE(typeid(L4::HashTable::ReadWrite::Serializer<HashTable>) == typeid(Serializer));
    }

    BOOST_CHECK(!reader.IsValid());

    auto newHashTableHolder = deserializer.Deserialize(memory, reader);

    BOOST_CHECK(reader.IsValid());
    BOOST_CHECK(newHashTableHolder != nullptr);

    WritableHashTable<Allocator> newWritableHashTable(*newHashTableHolder, epochManager);

    const auto& newPerfData = newWritableHashTable.GetPerfData();

    Utils::ValidateCounters(newPerfData, expectedCounterValuesAfterDeserialization);

    // Make sure all the key/value pairs exist after deserialization.
    for (const auto& pair : keyValuePairs)
    {
        auto key = Utils::ConvertFromString<IReadOnlyHashTable::Key>(pair.first.c_str());
        IReadOnlyHashTable::Value val;
        BOOST_CHECK(newWritableHashTable.Get(key, val));
        BOOST_CHECK(Utils::ConvertToString(val) == pair.second);
    }
}


BOOST_AUTO_TEST_CASE(CurrentSerializerTest)
{
    ValidateSerializer(
        Current::Serializer<HashTable>{},
        Current::Deserializer<Memory, HashTable>{ L4::Utils::Properties{} },
        Current::c_version,
        {
            { "hello1", " world1" },
            { "hello2", " world2" },
            { "hello3", " world3" }
        },
        {
            { HashTablePerfCounter::RecordsCount, 3 },
            { HashTablePerfCounter::BucketsCount, 5 },
            { HashTablePerfCounter::TotalKeySize, 18 },
            { HashTablePerfCounter::TotalValueSize, 21 },
            { HashTablePerfCounter::RecordsCountLoadedFromSerializer, 0 },
            { HashTablePerfCounter::RecordsCountSavedFromSerializer, 0 }
        },
        {
            { HashTablePerfCounter::RecordsCount, 3 },
            { HashTablePerfCounter::BucketsCount, 5 },
            { HashTablePerfCounter::TotalKeySize, 18 },
            { HashTablePerfCounter::TotalValueSize, 21 },
            { HashTablePerfCounter::RecordsCountLoadedFromSerializer, 0 },
            { HashTablePerfCounter::RecordsCountSavedFromSerializer, 3 }
        },
        {
            { HashTablePerfCounter::RecordsCount, 3 },
            { HashTablePerfCounter::BucketsCount, 5 },
            { HashTablePerfCounter::TotalKeySize, 18 },
            { HashTablePerfCounter::TotalValueSize, 21 },
            { HashTablePerfCounter::RecordsCountLoadedFromSerializer, 3 },
            { HashTablePerfCounter::RecordsCountSavedFromSerializer, 0 }
        });
}


BOOST_AUTO_TEST_CASE(HashTableSerializeTest)
{
    // This test case tests end to end scenario using the HashTableSerializer.
    ValidateSerializer(
        Serializer<HashTable>{},
        Deserializer<Memory, HashTable>{ L4::Utils::Properties{} },
        0U,
        {
            { "hello1", " world1" },
            { "hello2", " world2" },
            { "hello3", " world3" }
        },
        {
            { HashTablePerfCounter::RecordsCount, 3 },
            { HashTablePerfCounter::BucketsCount, 5 },
            { HashTablePerfCounter::TotalKeySize, 18 },
            { HashTablePerfCounter::TotalValueSize, 21 },
            { HashTablePerfCounter::RecordsCountLoadedFromSerializer, 0 },
            { HashTablePerfCounter::RecordsCountSavedFromSerializer, 0 }
        },
        {
            { HashTablePerfCounter::RecordsCount, 3 },
            { HashTablePerfCounter::BucketsCount, 5 },
            { HashTablePerfCounter::TotalKeySize, 18 },
            { HashTablePerfCounter::TotalValueSize, 21 },
            { HashTablePerfCounter::RecordsCountLoadedFromSerializer, 0 },
            { HashTablePerfCounter::RecordsCountSavedFromSerializer, 3 }
        },
        {
            { HashTablePerfCounter::RecordsCount, 3 },
            { HashTablePerfCounter::BucketsCount, 5 },
            { HashTablePerfCounter::TotalKeySize, 18 },
            { HashTablePerfCounter::TotalValueSize, 21 },
            { HashTablePerfCounter::RecordsCountLoadedFromSerializer, 3 },
            { HashTablePerfCounter::RecordsCountSavedFromSerializer, 0 }
        });
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace UnitTests
} // namespace L4