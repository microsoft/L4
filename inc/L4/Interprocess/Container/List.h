#pragma once

#include <boost/interprocess/containers/list.hpp>

namespace L4
{
namespace Interprocess
{
namespace Container
{


template <typename T, typename Allocator>
using List = boost::interprocess::list<T, Allocator>;


} // namespace Container
} // namespace Interprocess
} // namespace L4