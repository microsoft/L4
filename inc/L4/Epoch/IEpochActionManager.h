#pragma once

#include <functional>

namespace L4 {

// IEpochActionManager interface exposes an API for registering an Action.
struct IEpochActionManager {
  using Action = std::function<void()>;

  virtual ~IEpochActionManager(){};

  // Register actions on the latest epoch in the queue and the action is
  // performed when the epoch is removed from the queue.
  virtual void RegisterAction(Action&& action) = 0;
};

}  // namespace L4