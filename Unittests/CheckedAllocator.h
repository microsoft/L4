#pragma once

#include <boost/test/unit_test.hpp>
#include <memory>
#include <set>

namespace L4 {
namespace UnitTests {

struct AllocationAddressHolder : public std::set<void*> {
  ~AllocationAddressHolder() { BOOST_REQUIRE(empty()); }
};

template <typename T = void>
class CheckedAllocator : public std::allocator<T> {
 public:
  using Base = std::allocator<T>;
  using pointer = typename Base::pointer;

  template <class U>
  struct rebind {
    typedef CheckedAllocator<U> other;
  };

  CheckedAllocator()
      : m_allocationAddresses{std::make_shared<AllocationAddressHolder>()} {}

  CheckedAllocator(const CheckedAllocator<T>&) = default;

  template <class U>
  CheckedAllocator(const CheckedAllocator<U>& other)
      : m_allocationAddresses{other.m_allocationAddresses} {}

  template <class U>
  CheckedAllocator<T>& operator=(const CheckedAllocator<U>& other) {
    m_allocationAddresses = other.m_allocationAddresses;
    return (*this);
  }

  pointer allocate(std::size_t count,
                   std::allocator<void>::const_pointer hint = 0) {
    auto address = Base::allocate(count, hint);
    BOOST_REQUIRE(m_allocationAddresses->insert(address).second);
    return address;
  }

  void deallocate(pointer ptr, std::size_t count) {
    BOOST_REQUIRE(m_allocationAddresses->erase(ptr) == 1);
    Base::deallocate(ptr, count);
  }

  std::shared_ptr<AllocationAddressHolder> m_allocationAddresses;
};

}  // namespace UnitTests
}  // namespace L4
