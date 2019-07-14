#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace L4 {
namespace Utils {

// NoOp is a function object that doesn't do anything.
struct NoOp {
  void operator()(...) {}
};

// RunningThread wraps around std::thread and repeatedly runs a given function
// after yielding for the given interval. Note that the destructor waits for the
// thread to stop.
template <typename CoreFunc, typename PrepFunc = NoOp>
class RunningThread {
 public:
  RunningThread(std::chrono::milliseconds interval,
                CoreFunc coreFunc,
                PrepFunc prepFunc = PrepFunc())
      : m_isRunning(),
        m_thread(&RunningThread::Start, this, interval, coreFunc, prepFunc) {}

  ~RunningThread() {
    m_isRunning.store(false);

    if (m_thread.joinable()) {
      m_thread.join();
    }
  }

  RunningThread(const RunningThread&) = delete;
  RunningThread& operator=(const RunningThread&) = delete;

 private:
  void Start(std::chrono::milliseconds interval,
             CoreFunc coreFunc,
             PrepFunc prepFunc) {
    m_isRunning.store(true);

    prepFunc();

    while (m_isRunning.load()) {
      coreFunc();

      std::this_thread::sleep_for(interval);
    }
  }

  std::atomic_bool m_isRunning;

  std::thread m_thread;
};

}  // namespace Utils
}  // namespace L4
