#include "Interprocess/Utils/Handle.h"
#include "Utils/Exception.h"
#include <boost/format.hpp>

namespace L4
{
namespace Interprocess
{
namespace Utils
{

// Handle class implementation.

Handle::Handle(HANDLE handle, bool verifyHandle)
    : m_handle{ Verify(handle, verifyHandle), ::CloseHandle }
{}


Handle::Handle(Handle&& other)
    : m_handle{ std::move(other.m_handle) }
{}


Handle::operator HANDLE() const
{
    return m_handle.get();
}


HANDLE Handle::Verify(HANDLE handle, bool verifyHandle) const
{
    if (handle == NULL || handle == INVALID_HANDLE_VALUE || verifyHandle)
    {
        auto error = ::GetLastError();
        if (error != ERROR_SUCCESS)
        {
            boost::format err("Invalid handle: %1%.");
            err % error;
            throw RuntimeException(err.str());
        }
    }

    return handle;
}

} // namespace Utils
} // namespace Interprocess
} // namespace L4
