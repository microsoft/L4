#pragma once

#include "Utils/Containers.h"

#include <boost/lexical_cast.hpp>

namespace L4 {
namespace Utils {

// Properties class represents a string to string map (case insensitive).
// It can be used where the configurations should be generic.
class Properties : public StdStringKeyMap<std::string> {
 public:
  using Base = Utils::StdStringKeyMap<std::string>;
  using Value = Base::value_type;

  Properties() = default;

  // Expose a constructor with initializer_list for convenience.
  Properties(std::initializer_list<Value> values) : Base(values) {}

  // Returns true if the given key exists and the value associated with
  // the key can be converted to the TValue type. If the conversion fails, the
  // value of the given val is guaranteed to remain the same.
  template <typename TValue>
  bool TryGet(const std::string& key, TValue& val) const {
    const auto it = find(key);
    if (it == end()) {
      return false;
    }

    TValue tmp;
    if (!boost::conversion::try_lexical_convert(it->second, tmp)) {
      return false;
    }

    val = tmp;

    return true;
  }
};

}  // namespace Utils
}  // namespace L4