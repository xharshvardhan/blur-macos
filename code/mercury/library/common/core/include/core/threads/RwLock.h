#ifndef __MERCURY_SYSTEM_RWLOCK_H__
#define __MERCURY_SYSTEM_RWLOCK_H__

#include <core/threads/Mutex.h>
#include <core/threads/Event.h>

#ifdef _MSC_VER
#pragma once
#endif

namespace MT
{
#if defined(PLATFORM_MACOS)
// [Blur2PC][macOS] Native timed reader/writer state.
class PlatformRwLock;
#endif

/// a read-write lock
/// allows multiple concurrent reads, or one write
class RwLock
{
public:
						/// constructor
					    RwLock();
						/// destructor
						~RwLock();

						/// acquire a read-lock
	bool				ReadLock(const U32 timeMs = WAIT_TIME_INFINITE );
						/// acquire a write-lock
	bool				WriteLock(const U32 timeMs = WAIT_TIME_INFINITE );
						/// release a read-lock
	void				ReleaseReadLock();
						/// release a write-lock
	void				ReleaseWriteLock();

private:
#if defined(PLATFORM_MACOS)
    // [Blur2PC][macOS] Avoid the original fallback's unsynchronized reader count.
    PlatformRwLock* m_Native;
    RwLock(const RwLock&) = delete;
    RwLock& operator=(const RwLock&) = delete;
#else
	Mutex				m_WriteMutex;		// exclusive write lock
	Event				m_ReadEvent;		// an event that is signaled when readers reaches 0
	S32					m_iReaders;			// number of concurrent reads
#endif
};

class ReadLock
{
	RwLock& m_Lock;
public:
	ReadLock(RwLock& lock) : m_Lock(lock)
	{
		m_Lock.ReadLock();
	}

	~ReadLock()
	{
		m_Lock.ReleaseReadLock();
	}
};

class WriteLock
{
	RwLock& m_Lock;
public:
	WriteLock(RwLock& lock) : m_Lock(lock)
	{
		m_Lock.WriteLock();
	}

	~WriteLock()
	{
		m_Lock.ReleaseWriteLock();
	}
};

}

#endif
