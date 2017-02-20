#pragma once

#include <memory>
#include <type_traits>
#include "Utils/Windows.h"

namespace L4
{
namespace Interprocess
{
namespace Utils
{

// Handle is a RAII class that manages the life time of the given HANDLE.
class Handle
{
public:
    // If verifyHandle is true, it checks whether a given handle is valid.
    explicit Handle(HANDLE handle, bool verifyHandle = false);

    Handle(Handle&& other);

    explicit operator HANDLE() const;

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle& operator=(Handle&&) = delete;

private:
    HANDLE Verify(HANDLE handle, bool verifyHandle) const;

    std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&::CloseHandle)> m_handle;
};

} // namespace Utils
} // namespace Interprocess
} // namespace L4
