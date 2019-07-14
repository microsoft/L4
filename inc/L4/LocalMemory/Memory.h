#pragma once

namespace L4 {
namespace LocalMemory {

// Simple local memory model that stores the given allocator object.
template <typename Alloc>
class Memory {
 public:
  using Allocator = Alloc;

  template <typename T>
  using UniquePtr = std::unique_ptr<T>;

  template <typename T>
  using Deleter = typename std::default_delete<T>;

  explicit Memory(Allocator allocator = Allocator()) : m_allocator{allocator} {}

  template <typename T, typename... Args>
  auto MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
  }

  Allocator GetAllocator() { return Allocator(m_allocator); }

  template <typename T>
  auto GetDeleter() {
    return Deleter<T>();
  }

  Memory(const Memory&) = delete;
  Memory& operator=(const Memory&) = delete;

 private:
  Allocator m_allocator;
};

}  // namespace LocalMemory
}  // namespace L4