#pragma once

#include <cstdint>
#include "HashTable/IHashTable.h"
#include "Utils/Exception.h"

namespace L4
{
namespace HashTable
{

// Record struct consists of key and value pair.
struct Record
{
    using Key = IReadOnlyHashTable::Key;
    using Value = IReadOnlyHashTable::Value;

    Record() = default;

    Record(
        const Key& key,
        const Value& value)
        : m_key{ key }
        , m_value{ value }
    {}

    Key m_key;
    Value m_value;
};


// RecordBuffer is a thin wrapper struct around a raw buffer array (pointer).
struct RecordBuffer
{
    std::uint8_t m_buffer[1];
};

static_assert(
    sizeof(RecordBuffer) == 1,
    "RecordBuffer size should be 1 to be a thin wrapper.");

// RecordSerializer provides a functionality to serialize/deserialize a record information.
class RecordSerializer
{
public:
    using Key = Record::Key;
    using Value = Record::Value;
    using KeySize = Key::size_type;
    using ValueSize = Value::size_type;

    RecordSerializer(
        KeySize fixedKeySize,
        ValueSize fixedValueSize,
        ValueSize metadataSize = 0U)
        : m_fixedKeySize{ fixedKeySize }
        , m_fixedValueSize{ fixedValueSize }
        , m_metadataSize{ metadataSize }
    {}

    // Returns the number of bytes needed for serializing the given key and value.
    std::size_t CalculateBufferSize(const Key& key, const Value& value) const
    {
        return
            ((m_fixedKeySize != 0)
                ? m_fixedKeySize
                : (key.m_size + sizeof(KeySize)))
            + ((m_fixedValueSize != 0)
                ? m_fixedValueSize + m_metadataSize
                : (value.m_size + sizeof(ValueSize) + m_metadataSize));
    }

    // Returns the number bytes used for key and value sizes.
    std::size_t CalculateRecordOverhead() const
    {
        return
            (m_fixedKeySize != 0 ? 0U : sizeof(KeySize))
            + (m_fixedValueSize != 0 ? 0U : sizeof(ValueSize));
    }

    // Serializes the given key and value to the given buffer.
    // Note that the buffer size is at least as big as the number of bytes
    // returned by CalculateBufferSize().
    RecordBuffer* Serialize(
        const Key& key,
        const Value& value,
        std::uint8_t* const buffer,
        std::size_t bufferSize) const
    {
        Validate(key, value);

        assert(CalculateBufferSize(key, value) <= bufferSize);
        (void)bufferSize;

        const auto start = SerializeSizes(buffer, key.m_size, value.m_size);

#if defined(_MSC_VER)
        memcpy_s(buffer + start, key.m_size, key.m_data, key.m_size);
        memcpy_s(buffer + start + key.m_size, value.m_size, value.m_data, value.m_size);
#else
        memcpy(buffer + start, key.m_data, key.m_size);
        memcpy(buffer + start + key.m_size, value.m_data, value.m_size);
#endif
        return reinterpret_cast<RecordBuffer*>(buffer);
    }

    // Serializes the given key, value and meta value to the given buffer.
    // The meta value is serialized between key and value.
    // Note that the buffer size is at least as big as the number of bytes
    // returned by CalculateBufferSize().
    RecordBuffer* Serialize(
        const Key& key,
        const Value& value,
        const Value& metaValue,
        std::uint8_t* const buffer,
        std::size_t bufferSize) const
    {
        Validate(key, value, metaValue);

        assert(CalculateBufferSize(key, value) <= bufferSize);
        (void)bufferSize;

        const auto start = SerializeSizes(buffer, key.m_size, value.m_size + metaValue.m_size);

#if defined(_MSC_VER)
        memcpy_s(buffer + start, key.m_size, key.m_data, key.m_size);
        memcpy_s(buffer + start + key.m_size, metaValue.m_size, metaValue.m_data, metaValue.m_size);
        memcpy_s(buffer + start + key.m_size + metaValue.m_size, value.m_size, value.m_data, value.m_size);
#else
        memcpy(buffer + start, key.m_data, key.m_size);
        memcpy(buffer + start + key.m_size, metaValue.m_data, metaValue.m_size);
        memcpy(buffer + start + key.m_size + metaValue.m_size, value.m_data, value.m_size);
#endif
        return reinterpret_cast<RecordBuffer*>(buffer);
    }

    // Deserializes the given buffer and returns a Record object.
    Record Deserialize(const RecordBuffer& buffer) const
    {
        Record record;

        const auto* dataBuffer = buffer.m_buffer;

        auto& key = record.m_key;
        if (m_fixedKeySize != 0)
        {
            key.m_size = m_fixedKeySize;
        }
        else
        {
            key.m_size = *reinterpret_cast<const KeySize*>(dataBuffer);
            dataBuffer += sizeof(KeySize);
        }

        auto& value = record.m_value;
        if (m_fixedValueSize != 0)
        {
            value.m_size = m_fixedValueSize + m_metadataSize;
        }
        else
        {
            value.m_size = *reinterpret_cast<const ValueSize*>(dataBuffer);
            dataBuffer += sizeof(ValueSize);
        }

        key.m_data = dataBuffer;
        value.m_data = dataBuffer + key.m_size;

        return record;
    }

private:
    // Validates key and value sizes when fixed sizes are set.
    // Throws an exception if invalid sizes are used.
    void Validate(const Key& key, const Value& value) const
    {
        if ((m_fixedKeySize != 0 && key.m_size != m_fixedKeySize)
            || (m_fixedValueSize != 0 && value.m_size != m_fixedValueSize))
        {
            throw RuntimeException("Invalid key or value sizes are given.");
        }
    }

    // Validates against the given meta value.
    void Validate(const Key& key, const Value& value, const Value& metaValue) const
    {
        Validate(key, value);

        if (m_metadataSize != metaValue.m_size)
        {
            throw RuntimeException("Invalid meta value size is given.");
        }
    }

    // Serializes size information to the given buffer.
    // It assumes that buffer has enough size for serialization.
    std::size_t SerializeSizes(
        std::uint8_t* const buffer,
        KeySize keySize,
        ValueSize valueSize) const
    {
        auto curBuffer = buffer;
        if (m_fixedKeySize == 0)
        {
            *reinterpret_cast<KeySize*>(curBuffer) = keySize;
            curBuffer += sizeof(keySize);
        }

        if (m_fixedValueSize == 0)
        {
            *reinterpret_cast<ValueSize*>(curBuffer) = valueSize;
            curBuffer += sizeof(valueSize);
        }

        return curBuffer - buffer;
    }

    const KeySize m_fixedKeySize;
    const ValueSize m_fixedValueSize;
    const ValueSize m_metadataSize;
};


} // namespace HashTable
} // namespace L4
