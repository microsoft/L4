#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <boost\functional\hash.hpp>


namespace L4
{
namespace Utils
{


// CaseInsensitiveStdStringComparer is a STL-compatible case-insensitive ANSI std::string comparer.
struct CaseInsensitiveStdStringComparer
{
    bool operator()(const std::string& str1, const std::string& str2) const
    {
        return _stricmp(str1.c_str(), str2.c_str()) == 0;
    }
};

// CaseInsensitiveStringComparer is a STL-compatible case-insensitive ANSI string comparer.
struct CaseInsensitiveStringComparer
{
    bool operator()(const char* const str1, const char* const str2) const
    {
        return _stricmp(str1, str2) == 0;
    }
};

// CaseInsensitiveStringHasher is a STL-compatible case-insensitive ANSI std::string hasher.
struct CaseInsensitiveStdStringHasher
{
    std::size_t operator()(const std::string& str) const
    {
        std::size_t seed = 0;

        for (auto c : str)
        {
            boost::hash_combine(seed, std::toupper(c));
        }

        return seed;
    }
};

// CaseInsensitiveStringHasher is a STL-compatible case-insensitive ANSI string hasher.
struct CaseInsensitiveStringHasher
{
    std::size_t operator()(const char* str) const
    {
        assert(str != nullptr);

        std::size_t seed = 0;

        while (*str)
        {
            boost::hash_combine(seed, std::toupper(*str++));
        }

        return seed;
    }
};


} // namespace Utils
} // namespace L4
