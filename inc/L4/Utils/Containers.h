#pragma once

#include <boost/functional/hash.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include "Utils/ComparerHasher.h"

namespace L4 {
namespace Utils {

// StdStringKeyMap is an unordered_map where the key is std::string. It is
// slower than StringKeyMap above, but it owns the memory of the string, so it's
// easier to use.
template <typename TValue>
using StdStringKeyMap =
    std::unordered_map<std::string,
                       TValue,
                       Utils::CaseInsensitiveStdStringHasher,
                       Utils::CaseInsensitiveStdStringComparer>;

// StringKeyMap is an unordered_map where the key is const char*.
// The memory of the key is not owned by StringKeyMap,
// but it is faster (than StdStringKeyMap below) for look up.
template <typename TValue>
using StringKeyMap = std::unordered_map<const char*,
                                        TValue,
                                        Utils::CaseInsensitiveStringHasher,
                                        Utils::CaseInsensitiveStringComparer>;

// IntegerKeyMap using boost::hash and std::equal_to comparer and hasher.
template <typename TKey, typename TValue>
using IntegerKeyMap =
    std::unordered_map<TKey, TValue, boost::hash<TKey>, std::equal_to<TKey>>;

}  // namespace Utils
}  // namespace L4