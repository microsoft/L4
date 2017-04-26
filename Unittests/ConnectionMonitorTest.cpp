#include <boost/test/unit_test.hpp>
#include <mutex>
#include <condition_variable>
#include "Utils.h"
#include "L4/Interprocess/Connection/ConnectionMonitor.h"
#include "L4/Interprocess/Connection/EndPointInfoUtils.h"

namespace L4
{
namespace UnitTests
{

BOOST_AUTO_TEST_SUITE(ConnectionMonitorTests)

BOOST_AUTO_TEST_CASE(ConnectionMonitorTest)
{
    std::vector<Interprocess::Connection::EndPointInfo> endPointsDisconnected;
    std::mutex lock;
    std::condition_variable cv;

    auto server = std::make_shared<Interprocess::Connection::ConnectionMonitor>();

    auto noOpCallback = [](const auto&) { throw std::runtime_error("This will not be called."); };
    auto callback = [&](const auto& endPoint)
    {
        std::unique_lock<std::mutex> guard{ lock };
        endPointsDisconnected.emplace_back(endPoint);
        cv.notify_one();
    };

    auto client1 = std::make_shared<Interprocess::Connection::ConnectionMonitor>();
    client1->Register(server->GetLocalEndPointInfo(), noOpCallback);
    server->Register(client1->GetLocalEndPointInfo(), callback);

    // Registering the same end point is not allowed.
    CHECK_EXCEPTION_THROWN_WITH_MESSAGE(
        server->Register(client1->GetLocalEndPointInfo(), noOpCallback); ,
        "Duplicate end point found.");

    auto client2 = std::make_shared<Interprocess::Connection::ConnectionMonitor>();
    client2->Register(server->GetLocalEndPointInfo(), callback);
    server->Register(client2->GetLocalEndPointInfo(), noOpCallback);

    auto client3 = std::make_shared<Interprocess::Connection::ConnectionMonitor>();
    client3->Register(server->GetLocalEndPointInfo(), callback);
    server->Register(client3->GetLocalEndPointInfo(), noOpCallback);

    BOOST_CHECK_EQUAL(server->GetRemoteConnectionsCount(), 3U);

    // Kill client1 and check if the callback is called on the server side.
    auto client1EndPointInfo = client1->GetLocalEndPointInfo();
    client1.reset();
    {
        std::unique_lock<std::mutex> guard{ lock };
        cv.wait(guard, [&] { return endPointsDisconnected.size() >= 1U; });
        BOOST_REQUIRE_EQUAL(endPointsDisconnected.size(), 1U);
        BOOST_CHECK(endPointsDisconnected[0] == client1EndPointInfo);
        endPointsDisconnected.clear();
        BOOST_CHECK_EQUAL(server->GetRemoteConnectionsCount(), 2U);
    }

    // Now kill server and check if both callbacks in client2 and client3 are called.
    auto serverEndPointInfo = server->GetLocalEndPointInfo();
    server.reset();
    {
        std::unique_lock<std::mutex> guard{ lock };
        cv.wait(guard, [&] { return endPointsDisconnected.size() >= 2U; });
        BOOST_REQUIRE_EQUAL(endPointsDisconnected.size(), 2U);
        BOOST_CHECK(endPointsDisconnected[0] == serverEndPointInfo);
        BOOST_CHECK(endPointsDisconnected[1] == serverEndPointInfo);
        endPointsDisconnected.clear();
        BOOST_CHECK_EQUAL(client2->GetRemoteConnectionsCount(), 0U);
        BOOST_CHECK_EQUAL(client3->GetRemoteConnectionsCount(), 0U);
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace UnitTests
} // namespace L4