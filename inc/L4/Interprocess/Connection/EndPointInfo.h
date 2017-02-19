#pragma once

#include <cstdint>
#include <boost/uuid/uuid.hpp>

namespace L4
{
namespace Interprocess
{
namespace Connection
{

// EndPointInfo struct encapsulates the connection end point
// information across process boundaries.
struct EndPointInfo
{
    explicit EndPointInfo(
        std::uint32_t pid = 0U,
        const boost::uuids::uuid& uuid = {})
        : m_pid{ pid }
        , m_uuid{ uuid }
    {}

    bool operator==(const EndPointInfo& other) const
    {
        return (m_pid == other.m_pid)
            && (m_uuid == other.m_uuid);
    }

    bool operator<(const EndPointInfo& other) const
    {
        return m_uuid < other.m_uuid;
    }

    std::uint32_t m_pid;
    boost::uuids::uuid m_uuid;
};

} // namespace Connection
} // namespace Interprocess
} // namespace L4
