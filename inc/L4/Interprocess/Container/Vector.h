#pragma once

#include <boost/interprocess/containers/vector.hpp>

namespace L4
{
namespace Interprocess
{
namespace Container
{


template <typename T, typename Allocator>
using Vector = boost::interprocess::vector<T, Allocator>;


} // namespace Container
} // namespace Interprocess
} // namespace L4