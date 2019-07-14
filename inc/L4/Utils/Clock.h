#pragma once

#include <chrono>

namespace L4 {
namespace Utils {

class EpochClock {
 public:
  std::chrono::seconds GetCurrentEpochTime() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
  }
};

}  // namespace Utils
}  // namespace L4
