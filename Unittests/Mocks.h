#pragma once

#include "stdafx.h"
#include "L4/Epoch/IEpochActionManager.h"
#include "L4/Log/PerfLogger.h"
#include "L4/Serialization/IStream.h"

namespace L4
{
namespace UnitTests
{

class MockPerfLogger : public IPerfLogger
{
    virtual void Log(const IData& data) override
    {
        (void)data;
    }
};

struct MockEpochManager : public IEpochActionManager
{
    MockEpochManager()
        : m_numRegisterActionsCalled(0)
    {
    }

    virtual void RegisterAction(Action&& action) override
    {
        ++m_numRegisterActionsCalled;
        action();
    };

    std::uint16_t m_numRegisterActionsCalled;
};

class StreamBase
{
public:
    using StreamBuffer = std::vector<std::uint8_t>;

protected:
    StreamBase() = default;

    void Begin()
    {
        m_isBeginCalled = !m_isBeginCalled;
        if (!m_isBeginCalled)
        {
            BOOST_FAIL("Begin() is called multiple times.");
        }
    }

    void End()
    {
        if (!m_isBeginCalled)
        {
            BOOST_FAIL("Begin() is not called yet.");
        }

        m_isEndCalled = !m_isEndCalled;
        if (!m_isEndCalled)
        {
            BOOST_FAIL("End() is called multiple times.");
        }
    }

    void Validate()
    {
        if (!m_isBeginCalled)
        {
            BOOST_FAIL("Begin() is not called yet.");
        }

        if (m_isEndCalled)
        {
            BOOST_FAIL("End() is already called.");
        }
    }

    bool IsValid() const
    {
        return m_isBeginCalled && m_isEndCalled;
    }

    bool m_isBeginCalled = false;
    bool m_isEndCalled = false;
};


class MockStreamWriter : public IStreamWriter, private StreamBase
{
public:
    virtual void Begin() override
    {
        StreamBase::Begin();
    }

    virtual void End() override
    {
        StreamBase::End();
    }

    virtual void Write(const std::uint8_t buffer[], std::size_t bufferSize) override
    {
        StreamBase::Validate();
        m_buffer.insert(m_buffer.end(), buffer, buffer + bufferSize);
    }

    bool IsValid() const
    {
        return StreamBase::IsValid();
    }

    const StreamBuffer& GetStreamBuffer() const
    {
        return m_buffer;
    }

private:
    StreamBuffer m_buffer;
};


class MockStreamReader : public IStreamReader, private StreamBase
{
public:
    explicit MockStreamReader(const StreamBuffer& buffer)
        : m_buffer(buffer),
          m_bufferIter(m_buffer.cbegin())
    {
    }
    
    virtual void Begin() override
    {
        StreamBase::Begin();
    }

    virtual void End() override
    {
        // Make sure every thing is read from stream.
        BOOST_REQUIRE(m_bufferIter == m_buffer.end());
        StreamBase::End();
    }

    virtual void Read(std::uint8_t buffer[], std::size_t bufferSize) override
    {
        StreamBase::Validate();
        std::copy(m_bufferIter, m_bufferIter + bufferSize, buffer);
        m_bufferIter += bufferSize;
    }

    bool IsValid() const
    {
        return StreamBase::IsValid();
    }

private:
    StreamBuffer m_buffer;
    StreamBuffer::const_iterator m_bufferIter;
};

} // namespace UnitTests
} // namespace L4