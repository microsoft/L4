#include "Interprocess/Connection/EndPointInfoUtils.h"
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "Utils/Windows.h"

namespace L4 {
namespace Interprocess {
namespace Connection {

// EndPointInfoFactory class implementation.

EndPointInfo EndPointInfoFactory::Create() const {
  return EndPointInfo{GetCurrentProcessId(),
                      boost::uuids::random_generator()()};
}

// StringConverter class implementation.

std::string StringConverter::operator()(const EndPointInfo& endPoint) const {
  return "[pid:" + std::to_string(endPoint.m_pid) + "," +
         "uuid:" + boost::uuids::to_string(endPoint.m_uuid) + "]";
}

}  // namespace Connection
}  // namespace Interprocess
}  // namespace L4
