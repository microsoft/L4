#include <boost/test/unit_test.hpp>
#include <limits>
#include "L4/Log/PerfLogger.h"

namespace L4 {
namespace UnitTests {

void CheckMinCounters(const HashTablePerfData& htPerfData) {
  const auto maxValue = (std::numeric_limits<std::int64_t>::max)();
  /// Check if the min counter values are correctly initialized to max value.
  BOOST_CHECK_EQUAL(htPerfData.Get(HashTablePerfCounter::MinValueSize),
                    maxValue);
  BOOST_CHECK_EQUAL(htPerfData.Get(HashTablePerfCounter::MinKeySize), maxValue);
}

BOOST_AUTO_TEST_CASE(PerfCountersTest) {
  enum class TestCounter { Counter = 0, Count };

  PerfCounters<TestCounter> perfCounters;

  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 0);

  perfCounters.Set(TestCounter::Counter, 10);
  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 10);

  perfCounters.Increment(TestCounter::Counter);
  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 11);

  perfCounters.Decrement(TestCounter::Counter);
  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 10);

  perfCounters.Add(TestCounter::Counter, 5);
  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 15);

  perfCounters.Subtract(TestCounter::Counter, 10);
  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 5);

  perfCounters.Max(TestCounter::Counter, 10);
  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 10);

  perfCounters.Max(TestCounter::Counter, 9);
  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 10);

  perfCounters.Min(TestCounter::Counter, 1);
  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 1);

  perfCounters.Min(TestCounter::Counter, 10);
  BOOST_CHECK_EQUAL(perfCounters.Get(TestCounter::Counter), 1);
}

BOOST_AUTO_TEST_CASE(PerfDataTest) {
  PerfData testPerfData;

  BOOST_CHECK(testPerfData.GetHashTablesPerfData().empty());

  HashTablePerfData htPerfData1;
  HashTablePerfData htPerfData2;
  HashTablePerfData htPerfData3;

  CheckMinCounters(htPerfData1);
  CheckMinCounters(htPerfData2);
  CheckMinCounters(htPerfData3);

  testPerfData.AddHashTablePerfData("HT1", htPerfData1);
  testPerfData.AddHashTablePerfData("HT2", htPerfData2);
  testPerfData.AddHashTablePerfData("HT3", htPerfData3);

  /// Update counters and check if they are correctly updated.
  htPerfData1.Set(HashTablePerfCounter::TotalKeySize, 10);
  htPerfData2.Set(HashTablePerfCounter::TotalKeySize, 20);
  htPerfData3.Set(HashTablePerfCounter::TotalKeySize, 30);

  // Check if the hash table perf data is correctly registered.
  const auto& hashTablesPerfData = testPerfData.GetHashTablesPerfData();
  BOOST_CHECK_EQUAL(hashTablesPerfData.size(), 3U);

  {
    auto htPerfDataIt = hashTablesPerfData.find("HT1");
    BOOST_REQUIRE(htPerfDataIt != hashTablesPerfData.end());
    BOOST_CHECK_EQUAL(
        htPerfDataIt->second.get().Get(HashTablePerfCounter::TotalKeySize), 10);
  }
  {
    auto htPerfDataIt = hashTablesPerfData.find("HT2");
    BOOST_REQUIRE(htPerfDataIt != hashTablesPerfData.end());
    BOOST_CHECK_EQUAL(
        htPerfDataIt->second.get().Get(HashTablePerfCounter::TotalKeySize), 20);
  }
  {
    auto htPerfDataIt = hashTablesPerfData.find("HT3");
    BOOST_REQUIRE(htPerfDataIt != hashTablesPerfData.end());
    BOOST_CHECK_EQUAL(
        htPerfDataIt->second.get().Get(HashTablePerfCounter::TotalKeySize), 30);
  }
}

}  // namespace UnitTests
}  // namespace L4
