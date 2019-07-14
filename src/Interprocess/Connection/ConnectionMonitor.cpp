#include "Interprocess/Connection/ConnectionMonitor.h"
#include <atomic>
#include "Interprocess/Connection/EndPointInfoUtils.h"
#include "Utils/Exception.h"
#include "Utils/Windows.h"

namespace L4 {
namespace Interprocess {
namespace Connection {

// ConnectionMonitor class implementation.

ConnectionMonitor::ConnectionMonitor()
    : m_localEndPoint{EndPointInfoFactory().Create()},
      m_localEvent{::CreateEvent(
          NULL,
          TRUE,  // Manual reset in order to notify all end points registered.
          FALSE,
          StringConverter()(m_localEndPoint).c_str())} {}

ConnectionMonitor::~ConnectionMonitor() {
  // Notify the remote endpoints.
  ::SetEvent(static_cast<HANDLE>(m_localEvent));
}

const EndPointInfo& ConnectionMonitor::GetLocalEndPointInfo() const {
  return m_localEndPoint;
}

std::size_t ConnectionMonitor::GetRemoteConnectionsCount() const {
  UnRegister();

  std::lock_guard<std::mutex> lock(m_mutexOnRemoteMonitors);
  return m_remoteMonitors.size();
}

void ConnectionMonitor::Register(const EndPointInfo& remoteEndPoint,
                                 Callback callback) {
  UnRegister();

  // The following is needed to prevent the case where the callback is trying
  // to call UnRegister() when the ConnectionMonitor is already destroyed.
  std::weak_ptr<ConnectionMonitor> thisWeakPtr = this->shared_from_this();

  // The following ensures that only one callback is triggered from one endpoint
  // even if we are waiting for two handles (process and event).
  auto isCalled = std::make_shared<std::atomic_bool>(false);

  std::lock_guard<std::mutex> lock(m_mutexOnRemoteMonitors);

  // Note that the following call may throw since opening handles may fail, but
  // it is exception safe (std::map::emplace has a strong guarantee on it).
  if (!m_remoteMonitors
           .emplace(remoteEndPoint,
                    std::make_unique<HandleMonitor>(
                        remoteEndPoint,
                        [thisWeakPtr, callback,
                         isCalled](const auto& remoteEndPoint) {
                          if (isCalled->exchange(true)) {
                            return;
                          }

                          callback(remoteEndPoint);
                          auto connectionMonitor = thisWeakPtr.lock();
                          if (connectionMonitor != nullptr) {
                            // Cannot call UnRegister() because it will
                            // self-destruct. Instead, call the UnRegister(const
                            // EndPointInfo&) and queue up the end point that
                            // will be removed from m_remoteEvents at a later
                            // time.
                            connectionMonitor->UnRegister(remoteEndPoint);
                          }
                        }))
           .second) {
    throw RuntimeException("Duplicate end point found.");
  }
}

void ConnectionMonitor::UnRegister(const EndPointInfo& remoteEndPoint) {
  std::lock_guard<std::mutex> lock(m_mutexOnUnregisteredEndPoints);
  m_unregisteredEndPoints.emplace_back(remoteEndPoint);
}

void ConnectionMonitor::UnRegister() const {
  std::vector<EndPointInfo> unregisteredEndPoints;
  {
    // It is possible that the erase() in the following block can
    // wait for the callback to finish (::WaitForThreadpoolWaitCallbacks).
    // Since the callback calls the UnRegister(const EndPointinfo&), it can
    // deadlock if this function holds the lock while calling the erase(). Thus,
    // copy the m_unregisteredEndPoints and release the lock before calling
    // erase() below.
    std::lock_guard<std::mutex> lock(m_mutexOnUnregisteredEndPoints);
    unregisteredEndPoints.swap(m_unregisteredEndPoints);
  }

  std::lock_guard<std::mutex> lock(m_mutexOnRemoteMonitors);
  for (const auto& endPoint : unregisteredEndPoints) {
    m_remoteMonitors.erase(endPoint);
  }
}

// ConnectionMonitor::HandleMonitor::HandleMonitor class implementation.

ConnectionMonitor::HandleMonitor::HandleMonitor(
    const EndPointInfo& remoteEndPoint,
    Callback callback)
    : m_eventWaiter{std::make_unique<Waiter>(
          Utils::Handle{::OpenEvent(SYNCHRONIZE,
                                    FALSE,
                                    StringConverter()(remoteEndPoint).c_str())},
          [callback, endPoint = remoteEndPoint] { callback(endPoint); })},
      m_processWaiter{std::make_unique<Waiter>(
          Utils::Handle{
              ::OpenProcess(SYNCHRONIZE, FALSE, remoteEndPoint.m_pid)},
          [callback, endPoint = remoteEndPoint] { callback(endPoint); })} {}

// ConnectionMonitor::HandleMonitor::Waiter class implementation.

ConnectionMonitor::HandleMonitor::Waiter::Waiter(Utils::Handle handle,
                                                 Callback callback)
    : m_handle{std::move(handle)},
      m_callback{callback},
      m_wait{::CreateThreadpoolWait(OnEvent, this, NULL),
             ::CloseThreadpoolWait} {
  ::SetThreadpoolWait(m_wait.get(), static_cast<HANDLE>(m_handle), NULL);
}

ConnectionMonitor::HandleMonitor::Waiter::~Waiter() {
  ::SetThreadpoolWait(m_wait.get(), NULL, NULL);

  ::WaitForThreadpoolWaitCallbacks(m_wait.get(), TRUE);
}

VOID CALLBACK ConnectionMonitor::HandleMonitor::Waiter::OnEvent(
    PTP_CALLBACK_INSTANCE /*instance*/,
    PVOID context,
    PTP_WAIT /*wait*/,
    TP_WAIT_RESULT waitResult) {
  if (waitResult == WAIT_OBJECT_0) {
    static_cast<Waiter*>(context)->m_callback();
  } else {
    throw std::runtime_error{"Unexpected wait result is received."};
  }
}

}  // namespace Connection
}  // namespace Interprocess
}  // namespace L4
