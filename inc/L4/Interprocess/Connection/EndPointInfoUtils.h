#pragma once

#include <string>
#include "Interprocess/Connection/EndPointInfo.h"

namespace L4
{
namespace Interprocess
{
namespace Connection
{

// EndPointInfoFactory creates an EndPointInfo object with the current
// process id and a random uuid.
class EndPointInfoFactory
{
public:
    EndPointInfo Create() const;
};


// StringConverter provides a functionality to convert EndPointInfo to a string.
class StringConverter
{
public:
    std::string operator()(const EndPointInfo& endPoint) const;
};

} // namespace Connection
} // namespace Interprocess
} // namespace L4
