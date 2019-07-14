#pragma once

#include <map>
#include "PerfCounter.h"

namespace L4 {

// IPerfLogger interface.
struct IPerfLogger {
  struct IData;

  virtual ~IPerfLogger() = default;

  virtual void Log(const IData& data) = 0;
};

// IPerfLogger::IData interface that provides access to ServerPerfData and the
// aggregated HashTablePerfData. Note that the user of IPerfLogger only needs to
// implement IPerfLogger since IPerfLogger::IData is implemented internally.
struct IPerfLogger::IData {
  using HashTablesPerfData =
      std::map<std::string, std::reference_wrapper<const HashTablePerfData>>;

  virtual ~IData() = default;

  virtual const ServerPerfData& GetServerPerfData() const = 0;

  virtual const HashTablesPerfData& GetHashTablesPerfData() const = 0;
};

}  // namespace L4