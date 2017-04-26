#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string.h>
#include <vector>
#include "L4/Log/PerfCounter.h"
#include "L4/Utils/Exception.h"

namespace L4
{
    namespace UnitTests
    {

        // Macro CHECK_EXCEPTION_THROWN

#define CHECK_EXCEPTION_THROWN(statement) \
do { \
    bool isExceptionThrown = false;\
    try \
    { \
        statement; \
    } \
    catch (const RuntimeException&) \
    { \
        isExceptionThrown = true; \
    } \
    BOOST_CHECK(isExceptionThrown); \
} while (0)


#define CHECK_EXCEPTION_THROWN_WITH_MESSAGE(statement, message) \
do { \
    bool isExceptionThrown = false; \
    std::string exceptionMsg; \
    try \
    { \
        statement; \
    } \
    catch (const RuntimeException& ex) \
    { \
        isExceptionThrown = true; \
        exceptionMsg = ex.what(); \
        BOOST_TEST_MESSAGE("Exception Message: " << exceptionMsg); \
    } \
    BOOST_CHECK(isExceptionThrown); \
    BOOST_CHECK(strcmp((message), exceptionMsg.c_str()) == 0); \
} while (0)


        // This will validate the given message is a prefix of the exception message.
#define CHECK_EXCEPTION_THROWN_WITH_PREFIX_MESSAGE(statement, message) \
do { \
    bool isExceptionThrown = false; \
    std::string exceptionMsg; \
    try \
    { \
        statement; \
    } \
    catch (const RuntimeException& ex) \
    { \
        isExceptionThrown = true; \
        exceptionMsg = ex.what(); \
        BOOST_TEST_MESSAGE("Exception Message: " << exceptionMsg); \
    } \
    BOOST_CHECK(isExceptionThrown); \
    BOOST_CHECK(exceptionMsg.compare(0, strlen(message), message) == 0); \
} while (0)


        namespace Utils
        {

            template <typename T>
            T ConvertFromString(const char* str)
            {
                return T(
                    reinterpret_cast<const std::uint8_t*>(str),
                    static_cast<typename T::size_type>(strlen(str)));
            }

            template <typename T>
            std::string ConvertToString(const T& t)
            {
                return std::string(reinterpret_cast<const char*>(t.m_data), t.m_size);
            }


            // Counter related validation util function.

            using ExpectedCounterValues
                = std::vector<
                std::pair<
                HashTablePerfCounter,
                typename PerfCounters<HashTablePerfCounter>::TValue>>;

            // Validate the given perfData against the expected counter value.
            void ValidateCounter(
                const HashTablePerfData& actual,
                HashTablePerfCounter perfCounter,
                PerfCounters<HashTablePerfCounter>::TValue expectedValue);

            // Validate the given perfData against the expected counter values.
            void ValidateCounters(
                const HashTablePerfData& actual,
                const ExpectedCounterValues& expected);

        } // namespace Utils
    } // namespace UnitTests
} // namespace L4
