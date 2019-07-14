#pragma once

#include <cassert>
#include <chrono>
#include <cstdint>

namespace L4 {
namespace HashTable {
namespace Cache {

// Metadata class that stores caching related data.
// It stores access bit to indicate whether a record is recently accessed
// as well as the epoch time when a record is created.
// Note that this works regardless of the alignment of the metadata passed in.
class Metadata {
 public:
  // Constructs Metadata with the current epoch time.
  Metadata(std::uint32_t* metadata, std::chrono::seconds curEpochTime)
      : Metadata{metadata} {
    *m_metadata = curEpochTime.count() & s_epochTimeMask;
  }

  explicit Metadata(std::uint32_t* metadata) : m_metadata{metadata} {
    assert(m_metadata != nullptr);
  }

  // Returns the stored epoch time.
  std::chrono::seconds GetEpochTime() const {
    // *m_metadata even on the not-aligned memory should be fine since
    // only the byte that contains the access bit is modified, and
    // byte read is atomic.
    return std::chrono::seconds{*m_metadata & s_epochTimeMask};
  }

  // Returns true if the stored epoch time is expired based
  // on the given current epoch time and time-to-live value.
  bool IsExpired(std::chrono::seconds curEpochTime,
                 std::chrono::seconds timeToLive) const {
    assert(curEpochTime >= GetEpochTime());
    return (curEpochTime - GetEpochTime()) > timeToLive;
  }

  // Returns true if the access status is on.
  bool IsAccessed() const { return !!(GetAccessByte() & s_accessSetMask); }

  // If "set" is true, turn on the access bit in the given metadata and store
  // it. If "set" is false, turn off the access bit. Returns true if the given
  // metadata's access bit was originally on.
  bool UpdateAccessStatus(bool set) {
    const auto isAccessBitOn = IsAccessed();

    // Set the bit only if the bit is not set, and vice versa.
    if (set != isAccessBitOn) {
      if (set) {
        GetAccessByte() |= s_accessSetMask;
      } else {
        GetAccessByte() &= s_accessUnsetMask;
      }
    }

    return isAccessBitOn;
  }

  static constexpr std::uint16_t c_metaDataSize = sizeof(std::uint32_t);

 private:
  std::uint8_t GetAccessByte() const {
    return reinterpret_cast<std::uint8_t*>(m_metadata)[s_accessBitByte];
  }

  std::uint8_t& GetAccessByte() {
    return reinterpret_cast<std::uint8_t*>(m_metadata)[s_accessBitByte];
  }

  // TODO: Create an endian test and assert it. (Works only on little endian).
  // The byte that contains the most significant bit.
  static constexpr std::uint8_t s_accessBitByte = 3U;

  // Most significant bit is set.
  static constexpr std::uint8_t s_accessSetMask = 1U << 7;
  static constexpr std::uint8_t s_accessUnsetMask = s_accessSetMask ^ 0xFF;

  // The rest of bits other than the most significant bit are set.
  static constexpr std::uint32_t s_epochTimeMask = 0x7FFFFFFF;

  // The most significant bit is a CLOCK bit. It is set to 1 upon access
  // and reset to 0 by the cache eviction.
  // The rest of the bits are used for storing the epoch time in seconds.
  std::uint32_t* m_metadata = nullptr;
};

}  // namespace Cache
}  // namespace HashTable
}  // namespace L4
