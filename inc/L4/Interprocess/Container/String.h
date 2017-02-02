#pragma once

#include <boost/interprocess/containers/string.hpp>

namespace L4
{
namespace Interprocess
{
namespace Container
{


template <typename Allocator>
using String = boost::interprocess::basic_string<char, std::char_traits<char>, Allocator>;


} // namespace Container
} // namespace Interprocess
} // namespace L4