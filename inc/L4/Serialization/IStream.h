#pragma once

#include <cstdint>
#include <cstddef>


namespace L4
{


// IStream interface.
struct IStream
{
    virtual ~IStream() {}

    virtual void Begin() = 0;

    virtual void End() = 0;
};


// IStreamReader interface.
struct IStreamReader : public IStream
{
    virtual void Read(std::uint8_t buffer[], std::size_t bufferSize) = 0;
};


// IStreamWriter interface.
struct IStreamWriter : public IStream
{
    virtual void Write(const std::uint8_t buffer[], std::size_t bufferSize) = 0;
};


} // namespace L4