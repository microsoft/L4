#include "Utils.h"
#include <boost/test/unit_test.hpp>

namespace L4 {
namespace UnitTests {
namespace Utils {

void ValidateCounter(const HashTablePerfData& actual,
                     HashTablePerfCounter perfCounter,
                     PerfCounters<HashTablePerfCounter>::TValue expectedValue) {
  BOOST_CHECK_MESSAGE(
      actual.Get(perfCounter) == expectedValue,
      c_hashTablePerfCounterNames[static_cast<std::size_t>(perfCounter)]
          << " counter: " << actual.Get(perfCounter)
          << " (actual) != " << expectedValue << " (expected).");
}

void ValidateCounters(const HashTablePerfData& actual,
                      const ExpectedCounterValues& expected) {
  for (const auto& expectedCounter : expected) {
    ValidateCounter(actual, expectedCounter.first, expectedCounter.second);
  }
}

}  // namespace Utils
}  // namespace UnitTests
}  // namespace L4
