#pragma once

#include <cstdint>
#include <iosfwd>
#include "Log/PerfCounter.h"
#include "Utils/Properties.h"

namespace L4 {

// IReadOnlyHashTable interface for read-only access to the hash table.
struct IReadOnlyHashTable {
  // Blob struct that represents a memory blob.
  template <typename TSize>
  struct Blob {
    using size_type = TSize;

    explicit Blob(const std::uint8_t* data = nullptr, size_type size = 0U)
        : m_data{data}, m_size{size} {
      static_assert(std::numeric_limits<size_type>::is_integer,
                    "size_type is not an integer.");
    }

    bool operator==(const Blob& other) const {
      return (m_size == other.m_size) && !memcmp(m_data, other.m_data, m_size);
    }

    bool operator!=(const Blob& other) const { return !(*this == other); }

    const std::uint8_t* m_data;
    size_type m_size;
  };

  using Key = Blob<std::uint16_t>;
  using Value = Blob<std::uint32_t>;

  struct IIterator;

  using IIteratorPtr = std::unique_ptr<IIterator>;

  virtual ~IReadOnlyHashTable() = default;

  virtual bool Get(const Key& key, Value& value) const = 0;

  virtual IIteratorPtr GetIterator() const = 0;

  virtual const HashTablePerfData& GetPerfData() const = 0;
};

// IReadOnlyHashTable::IIterator interface for the hash table iterator.
struct IReadOnlyHashTable::IIterator {
  virtual ~IIterator() = default;

  virtual void Reset() = 0;

  virtual bool MoveNext() = 0;

  virtual Key GetKey() const = 0;

  virtual Value GetValue() const = 0;
};

// IWritableHashTable interface for write access to the hash table.
struct IWritableHashTable : public virtual IReadOnlyHashTable {
  struct ISerializer;

  using ISerializerPtr = std::unique_ptr<ISerializer>;

  virtual void Add(const Key& key, const Value& value) = 0;

  virtual bool Remove(const Key& key) = 0;

  virtual ISerializerPtr GetSerializer() const = 0;
};

// IWritableHashTable::ISerializer interface for serializing hash table.
struct IWritableHashTable::ISerializer {
  virtual ~ISerializer() = default;

  virtual void Serialize(std::ostream& stream,
                         const Utils::Properties& properties) = 0;
};

}  // namespace L4