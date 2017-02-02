#pragma once

#include <cstdint>
#include "SharedMemoryHashTable/Utils/Windows.h"

namespace Ads
{
namespace DE
{
namespace SharedMemory
{
namespace Utils
{
namespace Time
{


// Returns the current high resolution system counter value.
inline std::uint64_t GetCurrentSystemCounter()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

// Returns how many ticks there are in the given resolution interval.
// Note that the given resolution interval is in the same unit as NtQueryTimerResolution(),
// which is 1/10000 ms. Thus, 10000 translates to 1 ms.
// Note that this function is based on boost::interprocess::ipcdetail::get_system_tick_in_highres_counts().
inline std::uint32_t GetSystemTicks(std::uint32_t resolutionInterval)
{
    // Frequency in counts per second.
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    std::int64_t femtoSecondsInOneCount = (1000000000000000LL - 1LL) / freq.QuadPart + 1LL;

    // Calculate the ticks count perf given resolution interval.
    return static_cast<std::uint32_t>(
        (static_cast<std::int64_t>(resolutionInterval) * 100000000LL - 1LL) / femtoSecondsInOneCount + 1LL);
}


} // namespace Time
} // namespace Utils
} // namespace SharedMemory
} // namespace DE
} // namespace Ads
