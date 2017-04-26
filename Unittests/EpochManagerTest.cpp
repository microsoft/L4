#include <atomic>
#include <boost/test/unit_test.hpp>
#include "Utils.h"
#include "L4/Epoch/EpochQueue.h"
#include "L4/Epoch/EpochActionManager.h"
#include "L4/LocalMemory/EpochManager.h"
#include "L4/Log/PerfCounter.h"
#include "L4/Utils/Lock.h"

namespace L4
{
namespace UnitTests
{

BOOST_AUTO_TEST_SUITE(EpochManagerTests)

BOOST_AUTO_TEST_CASE(EpochRefManagerTest)
{
    std::uint64_t currentEpochCounter = 5U;
    const std::uint32_t c_epochQueueSize = 100U;

    using EpochQueue = EpochQueue<
        boost::shared_lock_guard<L4::Utils::ReaderWriterLockSlim>,
        std::lock_guard<L4::Utils::ReaderWriterLockSlim>>;

    EpochQueue epochQueue(currentEpochCounter, c_epochQueueSize);

    // Initially the ref count at the current epoch counter should be 0.
    BOOST_CHECK_EQUAL(epochQueue.m_refCounts[currentEpochCounter], 0U);

    EpochRefManager<EpochQueue> epochManager(epochQueue);

    BOOST_CHECK_EQUAL(epochManager.AddRef(), currentEpochCounter);

    // Validate that a reference count is incremented at the current epoch counter.
    BOOST_CHECK_EQUAL(epochQueue.m_refCounts[currentEpochCounter], 1U);

    epochManager.RemoveRef(currentEpochCounter);

    // Validate that a reference count is back to 0.
    BOOST_CHECK_EQUAL(epochQueue.m_refCounts[currentEpochCounter], 0U);

    // Decrementing a reference counter when it is already 0 will result in an exception.
    CHECK_EXCEPTION_THROWN_WITH_MESSAGE(
        epochManager.RemoveRef(currentEpochCounter);,
        "Reference counter is invalid.");
}


BOOST_AUTO_TEST_CASE(EpochCounterManagerTest)
{
    std::uint64_t currentEpochCounter = 0U;
    const std::uint32_t c_epochQueueSize = 100U;

    using EpochQueue = EpochQueue<
        boost::shared_lock_guard<L4::Utils::ReaderWriterLockSlim>,
        std::lock_guard<L4::Utils::ReaderWriterLockSlim>>;

    EpochQueue epochQueue(currentEpochCounter, c_epochQueueSize);

    EpochCounterManager<EpochQueue> epochCounterManager(epochQueue);

    // If RemoveUnreferenceEpochCounters() is called when m_fonrtIndex and m_backIndex are
    // the same, it will just return either value.
    BOOST_CHECK_EQUAL(epochCounterManager.RemoveUnreferenceEpochCounters(), currentEpochCounter);

    // Add two epoch counts.
    ++currentEpochCounter;
    ++currentEpochCounter;
    epochCounterManager.AddNewEpoch();
    epochCounterManager.AddNewEpoch();

    BOOST_CHECK_EQUAL(epochQueue.m_frontIndex, 0U);
    BOOST_CHECK_EQUAL(epochQueue.m_backIndex, currentEpochCounter);
    BOOST_CHECK_EQUAL(epochQueue.m_refCounts[epochQueue.m_frontIndex], 0U);

    // Since the m_frontIndex's reference count was zero, it will be incremented
    // all the way to currentEpochCounter.
    BOOST_CHECK_EQUAL(epochCounterManager.RemoveUnreferenceEpochCounters(), currentEpochCounter);
    BOOST_CHECK_EQUAL(epochQueue.m_frontIndex, currentEpochCounter);
    BOOST_CHECK_EQUAL(epochQueue.m_backIndex, currentEpochCounter);

    EpochRefManager<EpochQueue> epochRefManager(epochQueue);

    // Now add a reference at the currentEpochCounter;
    const auto epochCounterReferenced = epochRefManager.AddRef();
    BOOST_CHECK_EQUAL(epochCounterReferenced, currentEpochCounter);

    // Calling RemoveUnreferenceEpochCounters() should just return currentEpochCounter
    // since m_frontIndex and m_backIndex is the same. (Not affected by adding a reference yet).
    BOOST_CHECK_EQUAL(epochCounterManager.RemoveUnreferenceEpochCounters(), currentEpochCounter);
    BOOST_CHECK_EQUAL(epochQueue.m_frontIndex, currentEpochCounter);
    BOOST_CHECK_EQUAL(epochQueue.m_backIndex, currentEpochCounter);

    // Add one epoch count.
    ++currentEpochCounter;
    epochCounterManager.AddNewEpoch();

    // Now RemoveUnreferenceEpochCounters() should return epochCounterReferenced because
    // of the reference count.
    BOOST_CHECK_EQUAL(epochCounterManager.RemoveUnreferenceEpochCounters(), epochCounterReferenced);
    BOOST_CHECK_EQUAL(epochQueue.m_frontIndex, epochCounterReferenced);
    BOOST_CHECK_EQUAL(epochQueue.m_backIndex, currentEpochCounter);

    // Remove the reference.
    epochRefManager.RemoveRef(epochCounterReferenced);

    // Now RemoveUnreferenceEpochCounters() should return currentEpochCounter and m_frontIndex
    // should be in sync with m_backIndex.
    BOOST_CHECK_EQUAL(epochCounterManager.RemoveUnreferenceEpochCounters(), currentEpochCounter);
    BOOST_CHECK_EQUAL(epochQueue.m_frontIndex, currentEpochCounter);
    BOOST_CHECK_EQUAL(epochQueue.m_backIndex, currentEpochCounter);
}


BOOST_AUTO_TEST_CASE(EpochActionManagerTest)
{
    EpochActionManager actionManager(2U);

    bool isAction1Called = false;
    bool isAction2Called = false;

    auto action1 = [&]() { isAction1Called = true; };
    auto action2 = [&]() { isAction2Called = true; };

    // Register action1 and action2 at epoch count 5 and 6 respectively.
    actionManager.RegisterAction(5U, action1);
    actionManager.RegisterAction(6U, action2);

    BOOST_CHECK(!isAction1Called && !isAction2Called);

    actionManager.PerformActions(4);
    BOOST_CHECK(!isAction1Called && !isAction2Called);

    actionManager.PerformActions(5);
    BOOST_CHECK(!isAction1Called && !isAction2Called);

    actionManager.PerformActions(6);
    BOOST_CHECK(isAction1Called && !isAction2Called);

    actionManager.PerformActions(7);
    BOOST_CHECK(isAction1Called && isAction2Called);
}


BOOST_AUTO_TEST_CASE(EpochManagerTest)
{
    ServerPerfData perfData;
    LocalMemory::EpochManager epochManager(
        EpochManagerConfig(100000U, std::chrono::milliseconds(5U), 1U),
        perfData);

    std::atomic<bool> isActionCalled{ false };
    auto action = [&]() { isActionCalled = true; };

    auto epochCounterReferenced = epochManager.GetEpochRefManager().AddRef();

    epochManager.RegisterAction(action);

    // Justification for using sleep_for in unit tests:
    // - EpochManager already uses an internal thread which wakes up and perform a task
    // in a given interval and when the class is destroyed, there is a mechanism for
    // waiting for the thread anyway. It's more crucial to test the end to end scenario this way.
    // - The overall execution time for this test is less than 50 milliseconds.
    auto initialEpochCounter = perfData.Get(ServerPerfCounter::LatestEpochCounterInQueue);
    while (perfData.Get(ServerPerfCounter::LatestEpochCounterInQueue) - initialEpochCounter < 2)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    BOOST_CHECK(!isActionCalled);

    epochManager.GetEpochRefManager().RemoveRef(epochCounterReferenced);

    initialEpochCounter = perfData.Get(ServerPerfCounter::LatestEpochCounterInQueue);
    while (perfData.Get(ServerPerfCounter::LatestEpochCounterInQueue) - initialEpochCounter < 2)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    BOOST_CHECK(isActionCalled);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace UnitTests
} // namespace L4
