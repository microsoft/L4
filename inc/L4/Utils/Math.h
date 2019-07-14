#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>

namespace L4 {
namespace Utils {
namespace Math {

// Rounds up the number to the nearest multiple of base.
inline std::uint64_t RoundUp(std::uint64_t number, std::uint64_t base) {
  return base ? (((number + base - 1) / base) * base) : number;
}

// Rounds down the number to the nearest multiple of base.
inline std::uint64_t RoundDown(std::uint64_t number, std::uint64_t base) {
  return base ? ((number / base) * base) : number;
}

// Returns true if the given number is a power of 2.
inline bool IsPowerOfTwo(std::uint64_t number) {
  return number && ((number & (number - 1)) == 0);
}

// Returns the next highest power of two from the given value.
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2.
inline std::uint32_t NextHighestPowerOfTwo(std::uint32_t val) {
  --val;
  val |= val >> 1;
  val |= val >> 2;
  val |= val >> 4;
  val |= val >> 8;
  val |= val >> 16;
  return ++val;
}

// Provides utility functions doing pointer related arithmetics.
namespace PointerArithmetic {

// Returns a new pointer after adding an offset.
template <typename T>
inline T* Add(T* ptr, std::size_t offset) {
  return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(ptr) + offset);
}

// Returns a new pointer after subtracting an offset.
template <typename T>
inline T* Subtract(T* ptr, std::size_t offset) {
  return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(ptr) - offset);
}

// Returns the absolute value of difference in the number of bytes between two
// pointers.
inline std::size_t Distance(const void* lhs, const void* rhs) {
  return std::abs(reinterpret_cast<std::ptrdiff_t>(lhs) -
                  reinterpret_cast<std::ptrdiff_t>(rhs));
}

}  // namespace PointerArithmetic

}  // namespace Math
}  // namespace Utils
}  // namespace L4