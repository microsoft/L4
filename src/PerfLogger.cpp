#include "Log/PerfLogger.h"
#include <boost/format.hpp>
#include "Utils/Exception.h"

namespace L4 {

// PerfData class implementation.

void PerfData::AddHashTablePerfData(const char* hashTableName,
                                    const HashTablePerfData& perfData) {
  auto result = m_hashTablesPerfData.insert(
      std::make_pair(hashTableName, HashTablesPerfData::mapped_type(perfData)));

  if (!result.second) {
    boost::format err("Duplicate hash table name found: '%1%'.");
    err % hashTableName;
    throw RuntimeException(err.str());
  }
}

}  // namespace L4