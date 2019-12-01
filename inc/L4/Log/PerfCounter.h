#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>

namespace L4 {

enum class ServerPerfCounter : std::uint16_t {
  // Connection Manager
  ClientConnectionsCount = 0U,

  // EpochManager
  OldestEpochCounterInQueue,
  LatestEpochCounterInQueue,
  PendingActionsCount,
  LastPerformedActionsCount,

  Count
};

const std::array<const char*,
                 static_cast<std::uint16_t>(ServerPerfCounter::Count)>
    c_serverPerfCounterNames = {
        // Connection Manager
        "ClientConnectionsCount",

        // EpochManager
        "OldestEpochCounterInQueue", "LatestEpochCounterInQueue",
        "PendingActionsCount", "LastPerformedActionsCount"};

enum class HashTablePerfCounter : std::uint16_t {
  RecordsCount = 0U,
  BucketsCount,
  TotalKeySize,
  TotalValueSize,
  TotalIndexSize,
  ChainingEntriesCount,

  // Max/Min counters are always increasing. In other words, we don't keep track
  // of the next max record size, when the max record is deleted.
  MinKeySize,
  MaxKeySize,
  MinValueSize,
  MaxValueSize,
  MaxBucketChainLength,

  RecordsCountLoadedFromSerializer,
  RecordsCountSavedFromSerializer,

  // CacheHashTable specific counters.
  CacheHitCount,
  CacheMissCount,
  EvictedRecordsCount,

  Count
};

const std::array<const char*,
                 static_cast<std::uint16_t>(HashTablePerfCounter::Count)>
    c_hashTablePerfCounterNames = {"RecordsCount",
                                   "BucketsCount",
                                   "TotalKeySize",
                                   "TotalValueSize",
                                   "TotalIndexSize",
                                   "ChainingEntriesCount",
                                   "MinKeySize",
                                   "MaxKeySize",
                                   "MinValueSize",
                                   "MaxValueSize",
                                   "MaxBucketChainLength",
                                   "RecordsCountLoadedFromSerializer",
                                   "RecordsCountSavedFromSerializer",
                                   "CacheHitCount",
                                   "CacheMissCount",
                                   "EvictedRecordsCount"};

template <typename TCounterEnum>
class PerfCounters {
 public:
  typedef std::int64_t TValue;
  typedef std::atomic<TValue> TCounter;

  PerfCounters() {
    std::for_each(std::begin(m_counters), std::end(m_counters),
                  [](TCounter& counter) { counter = 0; });
  }

  // Note that since the ordering doesn't matter when the counter is updated,
  // memory_order_relaxed is used for all perf counter updates. More from
  // http://en.cppreference.com/w/cpp/atomic/memory_order: Typical use for
  // relaxed memory ordering is updating counters, such as the reference
  // counters of std::shared_ptr, since this only requires atomicity, but not
  // ordering or synchronization.
  TValue Get(TCounterEnum counterEnum) const {
    return m_counters[static_cast<std::uint16_t>(counterEnum)].load(
        std::memory_order_relaxed);
  }

  void Set(TCounterEnum counterEnum, TValue value) {
    m_counters[static_cast<std::uint16_t>(counterEnum)].store(
        value, std::memory_order_relaxed);
  }

  void Increment(TCounterEnum counterEnum) {
    m_counters[static_cast<std::uint16_t>(counterEnum)].fetch_add(
        1, std::memory_order_relaxed);
  }

  void Decrement(TCounterEnum counterEnum) {
    m_counters[static_cast<std::uint16_t>(counterEnum)].fetch_sub(
        1, std::memory_order_relaxed);
  }

  void Add(TCounterEnum counterEnum, TValue value) {
    if (value != 0) {
      m_counters[static_cast<std::uint16_t>(counterEnum)].fetch_add(
          value, std::memory_order_relaxed);
    }
  }

  void Subtract(TCounterEnum counterEnum, TValue value) {
    if (value != 0) {
      m_counters[static_cast<std::uint16_t>(counterEnum)].fetch_sub(
          value, std::memory_order_relaxed);
    }
  }

  void Max(TCounterEnum counterEnum, TValue value) {
    auto& counter = m_counters[static_cast<std::uint16_t>(counterEnum)];

    TValue startValue = counter.load(std::memory_order_acquire);

    do {
      // "load()" from counter is needed only once since the value of Max is
      // monotonically increasing. If startValue is changed by other threads,
      // compare_exchange_strong will return false and startValue will be
      // written to the latest value, thus returning to this code path.
      if (startValue > value) {
        return;
      }
    } while (!counter.compare_exchange_strong(startValue, value,
                                              std::memory_order_release,
                                              std::memory_order_acquire));
  }

  void Min(TCounterEnum counterEnum, TValue value) {
    auto& counter = m_counters[static_cast<std::uint16_t>(counterEnum)];

    TValue startValue = counter.load(std::memory_order_acquire);
    do {
      // Check the comment in Max() and Min() is monotonically decreasing.
      if (startValue < value) {
        return;
      }
    } while (!counter.compare_exchange_strong(startValue, value,
                                              std::memory_order_release,
                                              std::memory_order_acquire));
  }

 private:
#if defined(_MSC_VER)
  __declspec(align(8)) TCounter m_counters[TCounterEnum::Count];
#else
#if defined(__GNUC__)
  TCounter m_counters[static_cast<size_t>(TCounterEnum::Count)]
      __attribute__((aligned(8)));
#endif
#endif
};

typedef PerfCounters<ServerPerfCounter> ServerPerfData;

struct HashTablePerfData : public PerfCounters<HashTablePerfCounter> {
  HashTablePerfData() {
    // Initialize any min counters to the max value.
    constexpr auto maxValue =
        (std::numeric_limits<HashTablePerfData::TValue>::max)();

    Set(HashTablePerfCounter::MinValueSize, maxValue);
    Set(HashTablePerfCounter::MinKeySize, maxValue);

    // MaxBucketChainLength starts with 1 since bucket already
    // contains the entry which stores the data.
    Set(HashTablePerfCounter::MaxBucketChainLength, 1);
  }
};

}  // namespace L4
