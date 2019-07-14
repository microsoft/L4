#include <algorithm>
#include <boost/test/unit_test.hpp>
#include <sstream>
#include "CheckedAllocator.h"
#include "L4/HashTable/Cache/HashTable.h"
#include "L4/HashTable/Cache/Metadata.h"
#include "L4/HashTable/Common/Record.h"
#include "Mocks.h"
#include "Utils.h"

namespace L4 {
namespace UnitTests {

using namespace HashTable::Cache;
using namespace std::chrono;

class MockClock {
 public:
  MockClock() = default;

  seconds GetCurrentEpochTime() const { return s_currentEpochTime; }

  static void SetEpochTime(seconds time) { s_currentEpochTime = time; }

  static void IncrementEpochTime(seconds increment) {
    s_currentEpochTime += increment;
  }

 private:
  static seconds s_currentEpochTime;
};

seconds MockClock::s_currentEpochTime{0U};

class CacheHashTableTestFixture {
 public:
  using Allocator = CheckedAllocator<>;
  using CacheHashTable = WritableHashTable<Allocator, MockClock>;
  using ReadOnlyCacheHashTable = ReadOnlyHashTable<Allocator, MockClock>;
  using HashTable = CacheHashTable::HashTable;

  CacheHashTableTestFixture()
      : m_allocator{},
        m_hashTable{HashTable::Setting{100U}, m_allocator},
        m_epochManager{} {
    MockClock::SetEpochTime(seconds{0U});
  }

  CacheHashTableTestFixture(const CacheHashTableTestFixture&) = delete;
  CacheHashTableTestFixture& operator=(const CacheHashTableTestFixture&) =
      delete;

 protected:
  template <typename TCacheHashTable>
  bool Get(TCacheHashTable& hashTable,
           const std::string& key,
           IReadOnlyHashTable::Value& value) {
    return hashTable.Get(
        Utils::ConvertFromString<IReadOnlyHashTable::Key>(key.c_str()), value);
  }

  void Add(CacheHashTable& hashTable,
           const std::string& key,
           const std::string& value) {
    hashTable.Add(
        Utils::ConvertFromString<IReadOnlyHashTable::Key>(key.c_str()),
        Utils::ConvertFromString<IReadOnlyHashTable::Value>(value.c_str()));
  }

  void Remove(CacheHashTable& hashTable, const std::string& key) {
    hashTable.Remove(
        Utils::ConvertFromString<IReadOnlyHashTable::Key>(key.c_str()));
  }

  template <typename TCacheHashTable>
  bool CheckRecord(TCacheHashTable& hashTable,
                   const std::string& key,
                   const std::string& expectedValue) {
    IReadOnlyHashTable::Value value;
    return Get(hashTable, key, value) && AreTheSame(value, expectedValue);
  }

  bool AreTheSame(const IReadOnlyHashTable::Value& actual,
                  const std::string& expected) {
    return (actual.m_size == expected.size()) &&
           !memcmp(actual.m_data, expected.c_str(), actual.m_size);
  }

  template <typename Blob>
  bool Exist(const Blob& actual, const std::vector<std::string>& expectedSet) {
    const std::string actualStr(reinterpret_cast<const char*>(actual.m_data),
                                actual.m_size);

    return std::find(expectedSet.cbegin(), expectedSet.cend(), actualStr) !=
           expectedSet.cend();
  }

  Allocator m_allocator;
  HashTable m_hashTable;
  MockEpochManager m_epochManager;
  MockClock m_clock;
};

BOOST_AUTO_TEST_SUITE(CacheHashTableTests)

BOOST_AUTO_TEST_CASE(MetadataTest) {
  std::vector<std::uint8_t> buffer(20);

  // The following will test with 1..8 byte alignments.
  for (std::uint16_t i = 0U; i < 8U; ++i) {
    std::uint32_t* metadataBuffer =
        reinterpret_cast<std::uint32_t*>(buffer.data() + i);
    seconds currentEpochTime{0x7FABCDEF};

    Metadata metadata{metadataBuffer, currentEpochTime};

    BOOST_CHECK(currentEpochTime == metadata.GetEpochTime());

    // 10 seconds have elapsed.
    currentEpochTime += seconds{10U};

    // Check the expiration based on the time to live value.
    BOOST_CHECK(!metadata.IsExpired(currentEpochTime, seconds{15}));
    BOOST_CHECK(!metadata.IsExpired(currentEpochTime, seconds{10}));
    BOOST_CHECK(metadata.IsExpired(currentEpochTime, seconds{5U}));

    // Test access state.
    BOOST_CHECK(!metadata.IsAccessed());

    metadata.UpdateAccessStatus(true);
    BOOST_CHECK(metadata.IsAccessed());

    metadata.UpdateAccessStatus(false);
    BOOST_CHECK(!metadata.IsAccessed());
  }
}

BOOST_FIXTURE_TEST_CASE(ExpirationTest, CacheHashTableTestFixture) {
  // Don't care about evict in this test case, so make the cache size big.
  constexpr std::uint64_t c_maxCacheSizeInBytes = 0xFFFFFFFF;
  constexpr seconds c_recordTimeToLive{20U};

  CacheHashTable hashTable(m_hashTable, m_epochManager, c_maxCacheSizeInBytes,
                           c_recordTimeToLive, false);

  const std::vector<std::pair<std::string, std::string>> c_keyValuePairs = {
      {"key1", "value1"},
      {"key2", "value2"},
      {"key3", "value3"},
      {"key4", "value4"},
      {"key5", "value5"}};

  // Add 5 records at a different epoch time (10 seconds increment).
  for (const auto& pair : c_keyValuePairs) {
    MockClock::IncrementEpochTime(seconds{10});
    Add(hashTable, pair.first, pair.second);

    // Make sure the records can be retrieved right away. The record has not
    // been expired since the clock hasn't moved yet.
    BOOST_CHECK(CheckRecord(hashTable, pair.first, pair.second));
  }

  const auto& perfData = hashTable.GetPerfData();
  Utils::ValidateCounters(perfData, {{HashTablePerfCounter::CacheHitCount, 5}});

  // Now we have the following data sets:
  // | Key  | Value  | Creation time |
  // | key1 | value1 | 10           |
  // | key2 | value2 | 20           |
  // | key3 | value3 | 30           |
  // | key4 | value4 | 40           |
  // | key5 | value5 | 50           |
  // And the current clock is at 50.

  // Do look ups and check expired records.
  for (const auto& pair : c_keyValuePairs) {
    IReadOnlyHashTable::Value value;
    // Our time to live value is 20, so key0 and key0 records should be expired.
    if (pair.first == "key1" || pair.first == "key2") {
      BOOST_CHECK(!Get(hashTable, pair.first, value));
    } else {
      BOOST_CHECK(CheckRecord(hashTable, pair.first, pair.second));
    }
  }

  Utils::ValidateCounters(perfData,
                          {{HashTablePerfCounter::CacheHitCount, 8},
                           {HashTablePerfCounter::CacheMissCount, 2}});

  MockClock::IncrementEpochTime(seconds{100});

  // All the records should be expired now.
  for (const auto& pair : c_keyValuePairs) {
    IReadOnlyHashTable::Value value;
    BOOST_CHECK(!Get(hashTable, pair.first, value));
  }

  Utils::ValidateCounters(perfData,
                          {{HashTablePerfCounter::CacheHitCount, 8},
                           {HashTablePerfCounter::CacheMissCount, 7}});
}

BOOST_FIXTURE_TEST_CASE(CacheHashTableIteratorTest, CacheHashTableTestFixture) {
  // Don't care about evict in this test case, so make the cache size big.
  constexpr std::uint64_t c_maxCacheSizeInBytes = 0xFFFFFFFF;
  constexpr seconds c_recordTimeToLive{20U};

  CacheHashTable hashTable(m_hashTable, m_epochManager, c_maxCacheSizeInBytes,
                           c_recordTimeToLive, false);

  const std::vector<std::string> c_keys = {"key1", "key2", "key3", "key4",
                                           "key5"};
  const std::vector<std::string> c_vals = {"val1", "val2", "val3", "val4",
                                           "val5"};

  // Add 5 records at a different epoch time (3 seconds increment).
  for (std::size_t i = 0; i < c_keys.size(); ++i) {
    MockClock::IncrementEpochTime(seconds{3});
    Add(hashTable, c_keys[i], c_vals[i]);
  }

  // Now we have the following data sets:
  // | Key  | Value  | Creation time |
  // | key1 | value1 | 3             |
  // | key2 | value2 | 6             |
  // | key3 | value3 | 9             |
  // | key4 | value4 | 12            |
  // | key5 | value5 | 15            |
  // And the current clock is at 15.

  auto iterator = hashTable.GetIterator();
  std::uint16_t numRecords = 0;
  while (iterator->MoveNext()) {
    ++numRecords;
    BOOST_CHECK(Exist(iterator->GetKey(), c_keys));
    BOOST_CHECK(Exist(iterator->GetValue(), c_vals));
  }

  BOOST_CHECK_EQUAL(numRecords, 5);

  // The clock becomes 30 and key1, key2 and key3 should expire.
  MockClock::IncrementEpochTime(seconds{15});

  iterator = hashTable.GetIterator();
  numRecords = 0;
  while (iterator->MoveNext()) {
    ++numRecords;
    BOOST_CHECK(
        Exist(iterator->GetKey(),
              std::vector<std::string>{c_keys.cbegin() + 2, c_keys.cend()}));
    BOOST_CHECK(
        Exist(iterator->GetValue(),
              std::vector<std::string>{c_vals.cbegin() + 2, c_vals.cend()}));
  }

  BOOST_CHECK_EQUAL(numRecords, 2);

  // The clock becomes 40 and all records should be expired now.
  MockClock::IncrementEpochTime(seconds{10});

  iterator = hashTable.GetIterator();
  while (iterator->MoveNext()) {
    BOOST_CHECK(false);
  }
}

BOOST_FIXTURE_TEST_CASE(TimeBasedEvictionTest, CacheHashTableTestFixture) {
  // We only care about time-based eviction in this test, so make the cache size
  // big.
  constexpr std::uint64_t c_maxCacheSizeInBytes = 0xFFFFFFFF;
  constexpr seconds c_recordTimeToLive{10U};

  // Hash table with one bucket makes testing the time-based eviction easy.
  HashTable internalHashTable{HashTable::Setting{1}, m_allocator};
  CacheHashTable hashTable(internalHashTable, m_epochManager,
                           c_maxCacheSizeInBytes, c_recordTimeToLive, true);

  const std::vector<std::pair<std::string, std::string>> c_keyValuePairs = {
      {"key1", "value1"},
      {"key2", "value2"},
      {"key3", "value3"},
      {"key4", "value4"},
      {"key5", "value5"}};

  for (const auto& pair : c_keyValuePairs) {
    Add(hashTable, pair.first, pair.second);
    BOOST_CHECK(CheckRecord(hashTable, pair.first, pair.second));
  }

  const auto& perfData = hashTable.GetPerfData();
  Utils::ValidateCounters(perfData,
                          {
                              {HashTablePerfCounter::CacheHitCount, 5},
                              {HashTablePerfCounter::RecordsCount, 5},
                              {HashTablePerfCounter::EvictedRecordsCount, 0},
                          });

  MockClock::IncrementEpochTime(seconds{20});

  // All the records should be expired now.
  for (const auto& pair : c_keyValuePairs) {
    IReadOnlyHashTable::Value value;
    BOOST_CHECK(!Get(hashTable, pair.first, value));
  }

  Utils::ValidateCounters(perfData,
                          {
                              {HashTablePerfCounter::CacheHitCount, 5},
                              {HashTablePerfCounter::CacheMissCount, 5},
                              {HashTablePerfCounter::RecordsCount, 5},
                              {HashTablePerfCounter::EvictedRecordsCount, 0},
                          });

  // Now try to add one record and all the expired records should be evicted.
  const auto& keyValuePair = c_keyValuePairs[0];
  Add(hashTable, keyValuePair.first, keyValuePair.second);

  Utils::ValidateCounters(perfData,
                          {
                              {HashTablePerfCounter::RecordsCount, 1},
                              {HashTablePerfCounter::EvictedRecordsCount, 5},
                          });
}

BOOST_FIXTURE_TEST_CASE(EvcitAllRecordsTest, CacheHashTableTestFixture) {
  const auto& perfData = m_hashTable.m_perfData;
  const auto initialTotalIndexSize =
      perfData.Get(HashTablePerfCounter::TotalIndexSize);
  const std::uint64_t c_maxCacheSizeInBytes = 500 + initialTotalIndexSize;
  constexpr seconds c_recordTimeToLive{5};

  CacheHashTable hashTable{m_hashTable, m_epochManager, c_maxCacheSizeInBytes,
                           c_recordTimeToLive, false};

  Utils::ValidateCounters(perfData,
                          {
                              {HashTablePerfCounter::EvictedRecordsCount, 0},
                          });

  const std::vector<std::pair<std::string, std::string>> c_keyValuePairs = {
      {"key1", "value1"},
      {"key2", "value2"},
      {"key3", "value3"},
      {"key4", "value4"},
      {"key5", "value5"}};

  for (const auto& pair : c_keyValuePairs) {
    Add(hashTable, pair.first, pair.second);
  }

  using L4::HashTable::RecordSerializer;

  // Variable key/value sizes.
  const auto recordOverhead =
      RecordSerializer{0U, 0U}.CalculateRecordOverhead();

  Utils::ValidateCounters(
      perfData,
      {
          {HashTablePerfCounter::RecordsCount, c_keyValuePairs.size()},
          {HashTablePerfCounter::TotalIndexSize,
           initialTotalIndexSize + (c_keyValuePairs.size() * recordOverhead)},
          {HashTablePerfCounter::EvictedRecordsCount, 0},
      });

  // Make sure all data records added are present and update the access status
  // for each record in order to test that accessed records are deleted when
  // it's under memory constraint.
  for (const auto& pair : c_keyValuePairs) {
    BOOST_CHECK(CheckRecord(hashTable, pair.first, pair.second));
  }

  // Now insert a record that will force all the records to be evicted due to
  // size.
  std::string bigRecordKeyStr(10, 'k');
  std::string bigRecordValStr(500, 'v');

  Add(hashTable, bigRecordKeyStr, bigRecordValStr);

  // Make sure all the previously inserted records are evicted.
  for (const auto& pair : c_keyValuePairs) {
    IReadOnlyHashTable::Value value;
    BOOST_CHECK(!Get(hashTable, pair.first, value));
  }

  // Make sure the big record is inserted.
  BOOST_CHECK(CheckRecord(hashTable, bigRecordKeyStr, bigRecordValStr));

  Utils::ValidateCounters(
      perfData,
      {
          {HashTablePerfCounter::RecordsCount, 1},
          {HashTablePerfCounter::TotalIndexSize,
           initialTotalIndexSize + (1 * recordOverhead)},
          {HashTablePerfCounter::EvictedRecordsCount, c_keyValuePairs.size()},
      });
}

BOOST_FIXTURE_TEST_CASE(EvcitRecordsBasedOnAccessStatusTest,
                        CacheHashTableTestFixture) {
  const std::uint64_t c_maxCacheSizeInBytes =
      2000 + m_hashTable.m_perfData.Get(HashTablePerfCounter::TotalIndexSize);
  const seconds c_recordTimeToLive{5};

  CacheHashTable hashTable(m_hashTable, m_epochManager, c_maxCacheSizeInBytes,
                           c_recordTimeToLive, false);

  constexpr std::uint32_t c_valueSize = 100;
  const std::string c_valStr(c_valueSize, 'v');
  const auto& perfData = hashTable.GetPerfData();
  std::uint16_t key = 1;

  while ((static_cast<std::uint64_t>(
              perfData.Get(HashTablePerfCounter::TotalIndexSize)) +
          perfData.Get(HashTablePerfCounter::TotalKeySize) +
          perfData.Get(HashTablePerfCounter::TotalValueSize) + c_valueSize) <
         c_maxCacheSizeInBytes) {
    std::stringstream ss;
    ss << "key" << key;
    Add(hashTable, ss.str(), c_valStr);
    ++key;
  }

  // Make sure no eviction happened.
  BOOST_CHECK_EQUAL(m_epochManager.m_numRegisterActionsCalled, 0U);

  // Look up with the "key1" key to update the access state.
  BOOST_CHECK(CheckRecord(hashTable, "key1", c_valStr));

  // Now add a new key, which triggers an eviction, but deletes other records
  // than the "key1" record.
  Add(hashTable, "newkey", c_valStr);

  // Now, eviction should have happened.
  BOOST_CHECK_GE(m_epochManager.m_numRegisterActionsCalled, 1U);

  // The "key1" record should not have been evicted.
  BOOST_CHECK(CheckRecord(hashTable, "key1", c_valStr));

  // Make sure the new key is actually added.
  BOOST_CHECK(CheckRecord(hashTable, "newkey", c_valStr));
}

// This is similar to the one in ReadWriteHashTableTest, but necessary since
// cache store adds the meta values.
BOOST_FIXTURE_TEST_CASE(FixedKeyValueHashTableTest, CacheHashTableTestFixture) {
  // Fixed 4 byte keys and 6 byte values.
  std::vector<HashTable::Setting> settings = {
      HashTable::Setting{100, 200, 4, 0}, HashTable::Setting{100, 200, 0, 6},
      HashTable::Setting{100, 200, 4, 6}};

  for (const auto& setting : settings) {
    // Don't care about evict in this test case, so make the cache size big.
    constexpr std::uint64_t c_maxCacheSizeInBytes = 0xFFFFFFFF;
    constexpr seconds c_recordTimeToLive{20U};

    HashTable hashTable{setting, m_allocator};
    CacheHashTable writableHashTable{hashTable, m_epochManager,
                                     c_maxCacheSizeInBytes, c_recordTimeToLive,
                                     false};

    ReadOnlyCacheHashTable readOnlyHashTable{hashTable, c_recordTimeToLive};

    constexpr std::uint8_t c_numRecords = 10;

    // Add records.
    for (std::uint8_t i = 0; i < c_numRecords; ++i) {
      Add(writableHashTable, "key" + std::to_string(i),
          "value" + std::to_string(i));
    }

    Utils::ValidateCounters(writableHashTable.GetPerfData(),
                            {{HashTablePerfCounter::RecordsCount, 10},
                             {HashTablePerfCounter::BucketsCount, 100},
                             {HashTablePerfCounter::TotalKeySize, 40},
                             {HashTablePerfCounter::TotalValueSize, 100},
                             {HashTablePerfCounter::MinKeySize, 4},
                             {HashTablePerfCounter::MaxKeySize, 4},
                             {HashTablePerfCounter::MinValueSize, 10},
                             {HashTablePerfCounter::MaxValueSize, 10}});

    // Validate all the records added.
    for (std::uint8_t i = 0; i < c_numRecords; ++i) {
      CheckRecord(readOnlyHashTable, "key" + std::to_string(i),
                  "value" + std::to_string(i));
    }

    // Remove first half of the records.
    for (std::uint8_t i = 0; i < c_numRecords / 2; ++i) {
      Remove(writableHashTable, "key" + std::to_string(i));
    }

    Utils::ValidateCounters(writableHashTable.GetPerfData(),
                            {{HashTablePerfCounter::RecordsCount, 5},
                             {HashTablePerfCounter::BucketsCount, 100},
                             {HashTablePerfCounter::TotalKeySize, 20},
                             {HashTablePerfCounter::TotalValueSize, 50}});

    // Verify the records.
    for (std::uint8_t i = 0; i < c_numRecords; ++i) {
      if (i < (c_numRecords / 2)) {
        IReadOnlyHashTable::Value value;
        BOOST_CHECK(!Get(readOnlyHashTable, "key" + std::to_string(i), value));
      } else {
        CheckRecord(readOnlyHashTable, "key" + std::to_string(i),
                    "value" + std::to_string(i));
      }
    }

    // Expire all the records.
    MockClock::IncrementEpochTime(seconds{100});

    // Verify the records.
    for (std::uint8_t i = 0; i < c_numRecords; ++i) {
      IReadOnlyHashTable::Value value;
      BOOST_CHECK(!Get(readOnlyHashTable, "key" + std::to_string(i), value));
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace UnitTests
}  // namespace L4
