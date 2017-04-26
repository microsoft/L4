#pragma once

#include <cstdint>
#include <boost/format.hpp>
#include <iosfwd>
#include "Epoch/IEpochActionManager.h"
#include "Log/PerfCounter.h"
#include "Serialization/SerializerHelper.h"
#include "Utils/Exception.h"
#include "Utils/Properties.h"

namespace L4
{
namespace HashTable
{
namespace ReadWrite
{

// Note that the HashTable template parameter in this file is
// HashTable::ReadWrite::ReadOnlyHashTable<Allocator>::HashTable.
// However, due to the cyclic dependency, it needs to be passed as a template type.


// All the deprecated (previous versions) serializer should be put inside the Deprecated namespace.
// Removing any of the Deprecated serializers from the source code will require the major package version change.
namespace Deprecated
{
} // namespace Deprecated


namespace Current
{

constexpr std::uint8_t c_version = 1U;

// Current serializer used for serializing hash tables.
// The serialization format of Serializer is:
// <Version Id = 1> <Hash table settings> followed by
// If the next byte is set to 1:
//     <Key size> <Key bytes> <Value size> <Value bytes>
// Otherwise, end of the records.
template <typename HashTable, template <typename> class ReadOnlyHashTable>
class Serializer
{
public:
    Serializer() = default;

    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;

    void Serialize(
        HashTable& hashTable,
        std::ostream& stream) const
    {
        auto& perfData = hashTable.m_perfData;
        perfData.Set(HashTablePerfCounter::RecordsCountSavedFromSerializer, 0);

        SerializerHelper helper(stream);

        helper.Serialize(c_version);

        helper.Serialize(&hashTable.m_setting, sizeof(hashTable.m_setting));

        ReadOnlyHashTable<typename HashTable::Allocator> readOnlyHashTable(hashTable);

        auto iterator = readOnlyHashTable.GetIterator();
        while (iterator->MoveNext())
        {
            helper.Serialize(true); // Indicates record exists.
            const auto key = iterator->GetKey();
            const auto value = iterator->GetValue();

            helper.Serialize(key.m_size);
            helper.Serialize(key.m_data, key.m_size);

            helper.Serialize(value.m_size);
            helper.Serialize(value.m_data, value.m_size);

            perfData.Increment(HashTablePerfCounter::RecordsCountSavedFromSerializer);
        }

        helper.Serialize(false); // Indicates the end of records.

        // Flush perf counter so that the values are up to date when GetPerfData() is called.
        std::atomic_thread_fence(std::memory_order_release);
    }
};

// Current Deserializer used for deserializing hash tables.
template <typename Memory, typename HashTable, template <typename> class WritableHashTable>
class Deserializer
{
public:
    explicit Deserializer(const Utils::Properties& /* properties */)
    {}

    Deserializer(const Deserializer&) = delete;
    Deserializer& operator=(const Deserializer&) = delete;

    typename Memory::template UniquePtr<HashTable> Deserialize(
        Memory& memory,
        std::istream& stream) const
    {
        DeserializerHelper helper(stream);

        typename HashTable::Setting setting;
        helper.Deserialize(setting);

        auto hashTable{ memory.template MakeUnique<HashTable>(
            setting,
            memory.GetAllocator()) };

        EpochActionManager epochActionManager;

        WritableHashTable<typename HashTable::Allocator> writableHashTable(
            *hashTable,
            epochActionManager);

        auto& perfData = hashTable->m_perfData;

        std::vector<std::uint8_t> keyBuffer;
        std::vector<std::uint8_t> valueBuffer;

        bool hasMoreData = false;
        helper.Deserialize(hasMoreData);

        while (hasMoreData)
        {
            IReadOnlyHashTable::Key key;
            IReadOnlyHashTable::Value value;

            helper.Deserialize(key.m_size);
            keyBuffer.resize(key.m_size);
            helper.Deserialize(keyBuffer.data(), key.m_size);
            key.m_data = keyBuffer.data();

            helper.Deserialize(value.m_size);
            valueBuffer.resize(value.m_size);
            helper.Deserialize(valueBuffer.data(), value.m_size);
            value.m_data = valueBuffer.data();

            writableHashTable.Add(key, value);

            helper.Deserialize(hasMoreData);

            perfData.Increment(HashTablePerfCounter::RecordsCountLoadedFromSerializer);
        }

        // Flush perf counter so that the values are up to date when GetPerfData() is called.
        std::atomic_thread_fence(std::memory_order_release);

        return hashTable;
    }

private:
    // Deserializer internally uses WritableHashTable for deserialization, therefore
    // an implementation of IEpochActionManager is needed. Since all the keys in the hash table
    // are expected to be unique, no RegisterAction() should be called.
    class EpochActionManager : public IEpochActionManager
    {
    public:
        void RegisterAction(Action&& /* action */) override
        {
            // Since it is assumed that the serializer is loading from the stream generated by the same serializer,
            // it is guaranteed that all the keys are unique (a property of a hash table). Therefore, RegisterAction()
            // should not be called by the WritableHashTable.
            throw RuntimeException("RegisterAction() should not be called from the serializer.");
        }
    };
};

} // namespace Current


// Serializer is the main driver for serializing a hash table.
// It always uses the Current::Serializer for serializing a hash table.
template <typename HashTable, template <typename> class ReadOnlyHashTable>
class Serializer
{
public:
    Serializer() = default;
    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;

    void Serialize(HashTable& hashTable, std::ostream& stream) const
    {
        Current::Serializer<HashTable, ReadOnlyHashTable>{}.Serialize(hashTable, stream);
    }
};

// Deserializer is the main driver for deserializing the input stream to create a hash table.
template <typename Memory, typename HashTable, template <typename> class WritableHashTable>
class Deserializer
{
public:
    explicit Deserializer(const Utils::Properties& properties)
        : m_properties(properties)
    {}

    Deserializer(const Deserializer&) = delete;
    Deserializer& operator=(const Deserializer&) = delete;

    typename Memory::template UniquePtr<HashTable> Deserialize(
        Memory& memory,
        std::istream& stream) const
    {
        std::uint8_t version = 0U;
        DeserializerHelper(stream).Deserialize(version);

        switch (version)
        {
        case Current::c_version:
            return Current::Deserializer<Memory, HashTable, WritableHashTable>{ m_properties }.Deserialize(memory, stream);
        default:
            boost::format err("Unsupported version '%1%' is given.");
            err % version;
            throw RuntimeException(err.str());
        }
    }

private:
    const Utils::Properties& m_properties;
};

} // namespace ReadWrite
} // namespace HashTable
} // namespace L4

