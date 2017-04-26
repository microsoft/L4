#pragma once

#include <atomic>
#include <cstdint>
#include <boost/version.hpp>
#include <boost/interprocess/offset_ptr.hpp>

namespace L4
{
namespace Utils
{


// AtomicOffsetPtr provides a way to atomically update the offset pointer.
// The current boost::interprocess::offset_ptr cannot be used with std::atomic<> because
// the class is not trivially copyable. AtomicOffsetPtr borrows the same concept to calculate
// the pointer address based on the offset (boost::interprocess::ipcdetail::offset_ptr_to* functions
// are reused).
// Note that ->, *, copy/assignment operators are not implemented intentionally so that
// the user (inside this library) is aware of what he is intended to do without accidentally
// incurring any performance hits.
template <typename T>
class AtomicOffsetPtr
{
public:
    AtomicOffsetPtr()
        : m_offset(1)
    {}

    AtomicOffsetPtr(const AtomicOffsetPtr&) = delete;
    AtomicOffsetPtr& operator=(const AtomicOffsetPtr&) = delete;

    T* Load(std::memory_order memoryOrder = std::memory_order_seq_cst) const
    {
        return static_cast<T*>(
            boost::interprocess::ipcdetail::offset_ptr_to_raw_pointer(
                this,
                m_offset.load(memoryOrder)));
    }

    void Store(T* ptr, std::memory_order memoryOrder = std::memory_order_seq_cst)
    {
#if defined(_MSC_VER)
        m_offset.store(boost::interprocess::ipcdetail::offset_ptr_to_offset(ptr, this), memoryOrder);
#else
        m_offset.store(boost::interprocess::ipcdetail::offset_ptr_to_offset<std::uintptr_t>(ptr, this), memoryOrder);
#endif
    }

private:
#if defined(_MSC_VER)
    std::atomic_uint64_t m_offset;
#else
    std::atomic<std::uint64_t> m_offset;
#endif
};


} // namespace Utils
} // namespace L4
