#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include "Interprocess/Connection/EndPointInfo.h"
#include "Interprocess/Utils/Handle.h"

namespace L4 {
namespace Interprocess {
namespace Connection {

// ConnectionMonitor monitors any registered end points.
// ConnectionMonitor creates a kernel event for local end point,
// and open process/event handles for any registered end points.
// When the remote endpoint's process closes, or its event handle
// is closed, the callback registered is triggered and the remote endpoint
// is removed from the ConnectionMonitor after the callback is finished..
class ConnectionMonitor
    : public std::enable_shared_from_this<ConnectionMonitor> {
 public:
  using Callback = std::function<void(const EndPointInfo&)>;

  ConnectionMonitor();
  ~ConnectionMonitor();

  const EndPointInfo& GetLocalEndPointInfo() const;

  std::size_t GetRemoteConnectionsCount() const;

  void Register(const EndPointInfo& remoteEndPoint, Callback callback);

  void UnRegister(const EndPointInfo& remoteEndPoint);

  ConnectionMonitor(const ConnectionMonitor&) = delete;
  ConnectionMonitor& operator=(const ConnectionMonitor&) = delete;

 private:
  class HandleMonitor;

  // UnRegister() removes the unregistered end points from m_remoteEvents.
  void UnRegister() const;

  const EndPointInfo m_localEndPoint;

  Utils::Handle m_localEvent;

  mutable std::map<EndPointInfo, std::unique_ptr<HandleMonitor>>
      m_remoteMonitors;

  mutable std::mutex m_mutexOnRemoteMonitors;

  mutable std::vector<EndPointInfo> m_unregisteredEndPoints;

  mutable std::mutex m_mutexOnUnregisteredEndPoints;
};

// ConnectionMonitor::HandleMonitor opens the given endpoint's process
// and event handles and waits for any event triggers.
class ConnectionMonitor::HandleMonitor {
 public:
  HandleMonitor(const EndPointInfo& remoteEndPoint, Callback callback);

  HandleMonitor(const HandleMonitor&) = delete;
  HandleMonitor& operator=(const HandleMonitor&) = delete;

 private:
  class Waiter;

  std::unique_ptr<Waiter> m_eventWaiter;
  std::unique_ptr<Waiter> m_processWaiter;
};

// ConnectionMonitor::HandleMonitor::Waiter waits on the given handle and calls
// the given callback when an event is triggered on the handle.
class ConnectionMonitor::HandleMonitor::Waiter {
 public:
  using Callback = std::function<void()>;

  Waiter(Utils::Handle handle, Callback callback);

  ~Waiter();

  Waiter(const Waiter&) = delete;
  Waiter& operator=(const Waiter&) = delete;

 private:
  static VOID CALLBACK OnEvent(PTP_CALLBACK_INSTANCE instance,
                               PVOID context,
                               PTP_WAIT wait,
                               TP_WAIT_RESULT waitResult);

  Utils::Handle m_handle;
  Callback m_callback;
  std::unique_ptr<TP_WAIT, decltype(&::CloseThreadpoolWait)> m_wait;
};

}  // namespace Connection
}  // namespace Interprocess
}  // namespace L4
