#pragma once

#include <stdexcept>

namespace L4
{

// RuntimeException class used across L4 library.
class RuntimeException : public std::runtime_error
{
public:
    explicit RuntimeException(const std::string& message)
        : std::runtime_error(message.c_str())
    {}

    explicit RuntimeException(const char* message)
        : std::runtime_error(message)
    {}
};

} // namespace L4
