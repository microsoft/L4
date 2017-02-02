#pragma once

#include "Utils/Windows.h"


namespace L4
{
namespace Utils
{


// Represents a RAII wrapper for Win32 CRITICAL_SECTION.
class CriticalSection : protected ::CRITICAL_SECTION
{
public:
    // Constructs and initializes the critical section.
    CriticalSection()
    {
        ::InitializeCriticalSection(this);
    }

    CriticalSection(const CriticalSection& other) = delete;
    CriticalSection& operator=(const CriticalSection& other) = delete;

    // Destructs the critical section.
    ~CriticalSection()
    {
        ::DeleteCriticalSection(this);
    }

    // Waits for ownership of the critical section.
    void lock()
    {
        ::EnterCriticalSection(this);
    }

    // Releases ownership of the critical section.
    void unlock()
    {
        ::LeaveCriticalSection(this);
    }
};

// Represents a RAII wrapper for Win32 SRW lock.
class ReaderWriterLockSlim
{
public:
    // Constructs and initializes an SRW lock.
    ReaderWriterLockSlim()
    {
        ::InitializeSRWLock(&m_lock);
    }

    ReaderWriterLockSlim(const ReaderWriterLockSlim& other) = delete;
    ReaderWriterLockSlim& operator=(const ReaderWriterLockSlim& other) = delete;

    // Acquires an SRW lock in shared mode.
    void lock_shared()
    {
        ::AcquireSRWLockShared(&m_lock);
    }

    // Acquires an SRW lock in exclusive mode.
    void lock()
    {
        ::AcquireSRWLockExclusive(&m_lock);
    }

    // Releases an SRW lock that was opened in shared mode.
    void unlock_shared()
    {
        ::ReleaseSRWLockShared(&m_lock);
    }

    // Releases an SRW lock that was opened in exclusive mode.
    void unlock()
    {
        ::ReleaseSRWLockExclusive(&m_lock);
    }

private:
    // Stores the Win32 SRW lock.
    ::SRWLOCK m_lock;
};


} // namespace Utils
} // namespace L4
