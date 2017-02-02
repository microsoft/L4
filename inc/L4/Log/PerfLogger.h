#pragma once

#include "IPerfLogger.h"

namespace L4
{


struct PerfLoggerManagerConfig;


// PerfData class, which holds the ServerPerfData and HashTablePerfData for each hash table.
// Note that PerfData owns the ServerPerfData but has only the const references to HashTablePerfData,
// which is owned by the HashTable.

class PerfData : public IPerfLogger::IData
{
public:
    PerfData() = default;

    ServerPerfData& GetServerPerfData();

    const ServerPerfData& GetServerPerfData() const override;

    const HashTablesPerfData& GetHashTablesPerfData() const override;

    void AddHashTablePerfData(const char* hashTableName, const HashTablePerfData& perfData);

    PerfData(const PerfData&) = delete;
    PerfData& operator=(const PerfData&) = delete;

private:
    ServerPerfData m_serverPerfData;
    HashTablesPerfData m_hashTablesPerfData;
};


// PerfData inline implementations.

inline ServerPerfData& PerfData::GetServerPerfData()
{
    return m_serverPerfData;
}

inline const ServerPerfData& PerfData::GetServerPerfData() const
{
    return m_serverPerfData;
}

inline const PerfData::HashTablesPerfData& PerfData::GetHashTablesPerfData() const
{
    return m_hashTablesPerfData;
}


} // namespace L4