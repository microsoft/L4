#pragma once

#include <cstdint>
#include "IStream.h"

namespace L4
{

// SerializerHelper provides help functions to write to IStreamWriter.
class SerializerHelper
{
public:
    SerializerHelper(IStreamWriter& writer)
        : m_writer(writer)
    {}

    SerializerHelper(const SerializerHelper&) = delete;
    SerializerHelper& operator=(const SerializerHelper&) = delete;

    template <typename T>
    void Serialize(const T& obj)
    {
        m_writer.Write(reinterpret_cast<const std::uint8_t*>(&obj), sizeof(obj));
    }

    void Serialize(const void* data, std::uint32_t dataSize)
    {
        m_writer.Write(static_cast<const std::uint8_t*>(data), dataSize);
    }

private:
    IStreamWriter& m_writer;
};


// DeserializerHelper provides help functions to read from IStreamReader.
class DeserializerHelper
{
public:
    DeserializerHelper(IStreamReader& reader)
        : m_reader(reader)
    {
    }

    DeserializerHelper(const DeserializerHelper&) = delete;
    DeserializerHelper& operator=(const DeserializerHelper&) = delete;

    template <typename T>
    void Deserialize(T& obj)
    {
        m_reader.Read(reinterpret_cast<std::uint8_t*>(&obj), sizeof(obj));
    }

    void Deserialize(void* data, std::uint32_t dataSize)
    {
        m_reader.Read(static_cast<std::uint8_t*>(data), dataSize);
    }

private:
    IStreamReader& m_reader;
};


} // namespace L4

