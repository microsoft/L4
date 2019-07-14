#include <boost/test/unit_test.hpp>
#include <sstream>
#include <string>
#include <vector>
#include "L4/HashTable/ReadWrite/HashTable.h"
#include "L4/HashTable/ReadWrite/Serializer.h"
#include "L4/LocalMemory/Memory.h"
#include "L4/Log/PerfCounter.h"
#include "Mocks.h"
#include "Utils.h"

namespace L4 {
namespace UnitTests {

using namespace HashTable::ReadWrite;

BOOST_AUTO_TEST_SUITE(HashTableSerializerTests)

using KeyValuePair = std::pair<std::string, std::string>;
using KeyValuePairs = std::vector<KeyValuePair>;
using Memory = LocalMemory::Memory<std::allocator<void>>;
using Allocator = typename Memory::Allocator;
using HashTable = WritableHashTable<Allocator>::HashTable;

template <typename Serializer, typename Deserializer>
void ValidateSerializer(
    const Serializer& serializer,
    const Deserializer& deserializer,
    std::uint8_t serializerVersion,
    const KeyValuePairs& keyValuePairs,
    const Utils::ExpectedCounterValues& expectedCounterValuesAfterLoad,
    const Utils::ExpectedCounterValues& expectedCounterValuesAfterSerialization,
    const Utils::ExpectedCounterValues&
        expectedCounterValuesAfterDeserialization) {
  Memory memory;
  MockEpochManager epochManager;

  auto hashTableHolder{memory.MakeUnique<HashTable>(HashTable::Setting{5},
                                                    memory.GetAllocator())};
  BOOST_CHECK(hashTableHolder != nullptr);

  WritableHashTable<Allocator> writableHashTable(*hashTableHolder,
                                                 epochManager);

  // Insert the given key/value pairs to the hash table.
  for (const auto& pair : keyValuePairs) {
    auto key =
        Utils::ConvertFromString<IReadOnlyHashTable::Key>(pair.first.c_str());
    auto val = Utils::ConvertFromString<IReadOnlyHashTable::Value>(
        pair.second.c_str());

    writableHashTable.Add(key, val);
  }

  const auto& perfData = writableHashTable.GetPerfData();

  Utils::ValidateCounters(perfData, expectedCounterValuesAfterLoad);

  // Now write the hash table to the stream.
  std::ostringstream outStream;
  serializer.Serialize(*hashTableHolder, outStream);
  Utils::ValidateCounters(perfData, expectedCounterValuesAfterSerialization);

  // Read in the hash table from the stream and validate it.
  std::istringstream inStream(outStream.str());

  // version == 0 means that it's run through the HashTableSerializer, thus the
  // following can be skipped.
  if (serializerVersion != 0) {
    std::uint8_t actualSerializerVersion = 0;
    DeserializerHelper(inStream).Deserialize(actualSerializerVersion);
    BOOST_CHECK(actualSerializerVersion == serializerVersion);
  } else {
    BOOST_REQUIRE(typeid(L4::HashTable::ReadWrite::Serializer<
                         HashTable, ReadOnlyHashTable>) == typeid(Serializer));
  }

  auto newHashTableHolder = deserializer.Deserialize(memory, inStream);
  BOOST_CHECK(newHashTableHolder != nullptr);

  WritableHashTable<Allocator> newWritableHashTable(*newHashTableHolder,
                                                    epochManager);

  const auto& newPerfData = newWritableHashTable.GetPerfData();

  Utils::ValidateCounters(newPerfData,
                          expectedCounterValuesAfterDeserialization);

  // Make sure all the key/value pairs exist after deserialization.
  for (const auto& pair : keyValuePairs) {
    auto key =
        Utils::ConvertFromString<IReadOnlyHashTable::Key>(pair.first.c_str());
    IReadOnlyHashTable::Value val;
    BOOST_CHECK(newWritableHashTable.Get(key, val));
    BOOST_CHECK(Utils::ConvertToString(val) == pair.second);
  }
}

BOOST_AUTO_TEST_CASE(CurrentSerializerTest) {
  ValidateSerializer(
      Current::Serializer<HashTable, ReadOnlyHashTable>{},
      Current::Deserializer<Memory, HashTable, WritableHashTable>{
          L4::Utils::Properties{}},
      Current::c_version,
      {{"hello1", " world1"}, {"hello2", " world2"}, {"hello3", " world3"}},
      {{HashTablePerfCounter::RecordsCount, 3},
       {HashTablePerfCounter::BucketsCount, 5},
       {HashTablePerfCounter::TotalKeySize, 18},
       {HashTablePerfCounter::TotalValueSize, 21},
       {HashTablePerfCounter::RecordsCountLoadedFromSerializer, 0},
       {HashTablePerfCounter::RecordsCountSavedFromSerializer, 0}},
      {{HashTablePerfCounter::RecordsCount, 3},
       {HashTablePerfCounter::BucketsCount, 5},
       {HashTablePerfCounter::TotalKeySize, 18},
       {HashTablePerfCounter::TotalValueSize, 21},
       {HashTablePerfCounter::RecordsCountLoadedFromSerializer, 0},
       {HashTablePerfCounter::RecordsCountSavedFromSerializer, 3}},
      {{HashTablePerfCounter::RecordsCount, 3},
       {HashTablePerfCounter::BucketsCount, 5},
       {HashTablePerfCounter::TotalKeySize, 18},
       {HashTablePerfCounter::TotalValueSize, 21},
       {HashTablePerfCounter::RecordsCountLoadedFromSerializer, 3},
       {HashTablePerfCounter::RecordsCountSavedFromSerializer, 0}});
}

BOOST_AUTO_TEST_CASE(HashTableSerializeTest) {
  // This test case tests end to end scenario using the HashTableSerializer.
  ValidateSerializer(
      Serializer<HashTable, ReadOnlyHashTable>{},
      Deserializer<Memory, HashTable, WritableHashTable>{
          L4::Utils::Properties{}},
      0U, {{"hello1", " world1"}, {"hello2", " world2"}, {"hello3", " world3"}},
      {{HashTablePerfCounter::RecordsCount, 3},
       {HashTablePerfCounter::BucketsCount, 5},
       {HashTablePerfCounter::TotalKeySize, 18},
       {HashTablePerfCounter::TotalValueSize, 21},
       {HashTablePerfCounter::RecordsCountLoadedFromSerializer, 0},
       {HashTablePerfCounter::RecordsCountSavedFromSerializer, 0}},
      {{HashTablePerfCounter::RecordsCount, 3},
       {HashTablePerfCounter::BucketsCount, 5},
       {HashTablePerfCounter::TotalKeySize, 18},
       {HashTablePerfCounter::TotalValueSize, 21},
       {HashTablePerfCounter::RecordsCountLoadedFromSerializer, 0},
       {HashTablePerfCounter::RecordsCountSavedFromSerializer, 3}},
      {{HashTablePerfCounter::RecordsCount, 3},
       {HashTablePerfCounter::BucketsCount, 5},
       {HashTablePerfCounter::TotalKeySize, 18},
       {HashTablePerfCounter::TotalValueSize, 21},
       {HashTablePerfCounter::RecordsCountLoadedFromSerializer, 3},
       {HashTablePerfCounter::RecordsCountSavedFromSerializer, 0}});
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace UnitTests
}  // namespace L4
