#pragma once

#include "L4/Epoch/IEpochActionManager.h"
#include "L4/Log/PerfLogger.h"

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

} // namespace UnitTests
} // namespace L4