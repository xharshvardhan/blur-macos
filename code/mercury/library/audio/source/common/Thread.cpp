#include "AudioPch.h"
#include "Thread.h"

#include <system/Perf/PerfEvent.h>

#if defined(PLATFORM_PC)
#include <winbase.h>
#endif // PLATFORM_PC

#ifdef _PS3
#include <sys/timer.h> // for sys_timer_usleep
#include <sys/ppu_thread.h> // for sys_ppu_thread_create
#include <sys/return_code.h> // for CELL_OK
#include <cell/atomic.h>
#endif // _PS3


// [Blur2PC] Darwin uses the standard joined worker lifecycle. Asynchronous
// suspension/termination has no safe equivalent and reports unsupported.
#if defined(PLATFORM_MACOS)
Audio::Core::Thread::Thread() : Thread(0,eNormal) {}
Audio::Core::Thread::Thread(U32 stack) : Thread(stack,eNormal) {}
Audio::Core::Thread::Thread(U32 stack,Priority priority) : m_pThreadContext(nullptr),m_uStackSize(stack),m_ePriority(priority),m_uExitCode(0) {}
Audio::Core::Thread::~Thread(){WaitUntilTerminated();}
bool Audio::Core::Thread::Start(void* context){
    if(m_NativeThread.joinable())return false;
    m_pThreadContext=context;m_uExitCode=0;
    try{m_NativeThread=std::thread(&Thread::EntryPoint,this);return true;}
    catch(const std::system_error&){return false;}
}
S32 Audio::Core::Thread::Suspend(){return -1;}
S32 Audio::Core::Thread::Resume(){return -1;}
S32 Audio::Core::Thread::Abort(){return -1;}
S32 Audio::Core::Thread::WaitUntilTerminated(){
    if(m_NativeThread.joinable()){
        if(m_NativeThread.get_id()==std::this_thread::get_id())return -1;
        m_NativeThread.join();
    }
    return S32(m_uExitCode);
}
bool Audio::Core::Thread::SetPriority(Priority priority){
    // No claimed real-time policy: only the platform's normal policy is supported.
    if(priority!=eNormal)return false;m_ePriority=priority;return true;
}
S32 Audio::Core::Thread::Run(){Setup();Execute(m_pThreadContext);return Shutdown();}
void Audio::Core::Thread::EntryPoint(Thread* worker){worker->m_uExitCode=U32(worker->Run());}
S32 Audio::Core::Thread::Increment(volatile S32* count){return __atomic_add_fetch(count,1,__ATOMIC_SEQ_CST);}
S32 Audio::Core::Thread::Decrement(volatile S32* count){return __atomic_sub_fetch(count,1,__ATOMIC_SEQ_CST);}
#else
////////////////////////////////////////////////////////////////////////////////
//	Thread
////////////////////////////////////////////////////////////////////////////////
Audio::Core::Thread::Thread()
{
	m_hThread = NULL;
	m_uStackSize = 0;
	m_pThreadContext = NULL;
	m_ePriority = eNormal;
#if defined (PLATFORM_PS3)
	#ifdef CFG_GOLD
	pthread_rwlock_init(&m_lock, NULL);
	#else
	if (0 != pthread_rwlock_init(&m_lock, NULL))
		printf("Thread couldn't aquire lock");
	#endif
#endif	//	PLATFORM_PS3
	m_uExitCode = 0;
}

////////////////////////////////////////////////////////////////////////////////
//	Thread
////////////////////////////////////////////////////////////////////////////////
Audio::Core::Thread::Thread(U32 uStackSize)
{
	m_hThread = NULL;
	m_pThreadContext = NULL;
	m_uStackSize = uStackSize;
	m_ePriority = eNormal;
#if defined (PLATFORM_PS3)
	#ifdef CFG_GOLD
	pthread_rwlock_init(&m_lock, NULL);
	#else
	if (0 != pthread_rwlock_init(&m_lock, NULL))
		printf("Thread couldn't aquire lock");
	#endif
#endif	//	PLATFORM_PS3
	m_uExitCode = 0;
}

////////////////////////////////////////////////////////////////////////////////
//	Thread
////////////////////////////////////////////////////////////////////////////////
Audio::Core::Thread::Thread(U32 uStackSize, Priority ePriority)
{
	m_hThread = NULL;
	m_pThreadContext = NULL;
	m_uStackSize = uStackSize;
	m_ePriority = ePriority;
	m_uExitCode = 0;
#if defined (PLATFORM_PS3) 
	#ifdef CFG_GOLD
	pthread_rwlock_init(&m_lock, NULL);
	#else
	if (0 != pthread_rwlock_init(&m_lock, NULL))
		printf("Thread couldn't aquire lock");
	#endif
#endif	//	PLATFORM_PS3
}

////////////////////////////////////////////////////////////////////////////////
//	Thread
////////////////////////////////////////////////////////////////////////////////
Audio::Core::Thread::~Thread()
{
#if defined (PLATFORM_PS3)
	#ifdef CFG_GOLD
	pthread_rwlock_init(&m_lock, NULL);
	#else
	if (0 != pthread_rwlock_destroy(&m_lock))
		printf("Thread couldn't destroy lock");
	#endif
#endif	//	PLATFORM_PS3
}

////////////////////////////////////////////////////////////////////////////////
//	Start
////////////////////////////////////////////////////////////////////////////////
bool Audio::Core::Thread::Start(void* pContext)
{
	if (m_hThread == NULL)
	{
		m_pThreadContext = pContext;

#if defined(PLATFORM_PC)
		m_hThread = CreateThread(NULL, 1024 * m_uStackSize, (LPTHREAD_START_ROUTINE)EntryPoint, this, CREATE_SUSPENDED, NULL);
		if (m_hThread == NULL)
			return false;
#else
	    if (CELL_OK != sys_ppu_thread_create(&m_hThread, EntryPoint, (uint64_t)this, m_ePriority, 0x10000, 0, "Audio"))
		    return false;
#endif
		else
		{

			if (!SetPriority(m_ePriority))
				return false;

#if defined(PLATFORM_PC)
			::ResumeThread(m_hThread);
#endif	//	PLATFORM_XENON
		}

		return true;
	}
	else
		return false;
}

////////////////////////////////////////////////////////////////////////////////
//	Start
////////////////////////////////////////////////////////////////////////////////
#ifdef PLATFORM_PC
S32 Audio::Core::Thread::Stop()
{
	if (m_hThread != NULL)
	{
		GetExitCodeThread(m_hThread, (DWORD*)&m_uExitCode);

		if (m_uExitCode == STILL_ACTIVE)
			::TerminateThread(m_hThread, DWORD(-1));

		CloseHandle(m_hThread);

		m_hThread = NULL;
	}

	return m_uExitCode;
}
#endif	//	PLATFORM_PC

////////////////////////////////////////////////////////////////////////////////
//	Suspend
////////////////////////////////////////////////////////////////////////////////
S32 Audio::Core::Thread::Suspend()
{
#if defined(PLATFORM_PC)
	if (m_hThread != NULL)
		return ::SuspendThread(m_hThread);
	else
		return -1;
#else
	return 0;
#endif // PLATFORM_XENON
}

////////////////////////////////////////////////////////////////////////////////
//	Resume
////////////////////////////////////////////////////////////////////////////////
S32 Audio::Core::Thread::Resume()
{
#if defined(PLATFORM_PC)
	if (m_hThread != NULL)
		return ::ResumeThread(m_hThread);
	else
		return -1;
#else
	return 0;
#endif // PLATFORM_XENON
}

////////////////////////////////////////////////////////////////////////////////
//	Abort
////////////////////////////////////////////////////////////////////////////////
S32 Audio::Core::Thread::Abort()
{
// TODO: other platforms
#ifdef PLATFORM_PS3
	sys_ppu_thread_exit(0);
#endif
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//	WaitUntilTerminated
////////////////////////////////////////////////////////////////////////////////
S32 Audio::Core::Thread::WaitUntilTerminated()
{
#ifdef PLATFORM_PS3
	uint64_t exitStatus;
	sys_ppu_thread_join(m_hThread, &exitStatus);
#endif

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//	SetPriority
////////////////////////////////////////////////////////////////////////////////
bool Audio::Core::Thread::SetPriority(Priority ePriority)
{
	bool bReturn = false;

#if defined(PLATFORM_PC)
	if (m_hThread != NULL)
	{
		bReturn = ::SetThreadPriority(m_hThread, (S32)ePriority) ? true : false;

		if (bReturn)
			m_ePriority = ePriority;
	}
#else
	sys_ppu_thread_set_priority( m_hThread, (S32)ePriority);

	return true;
#endif


	return bReturn;
}


////////////////////////////////////////////////////////////////////////////////
//	Start
////////////////////////////////////////////////////////////////////////////////
S32 Audio::Core::Thread::Run()
{
   Setup();

   Execute(m_pThreadContext);

   return Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
//	static EntryPoint
////////////////////////////////////////////////////////////////////////////////
#if defined(PLATFORM_PC)
S32 Audio::Core::Thread::EntryPoint(void* pThis)
{
   Thread* pThread = (Thread*)pThis;

   PERF_EVENT("Audio::Core::Thread");

   return pThread->Run();
}
#else
void Audio::Core::Thread::EntryPoint(uint64_t uParam)
{
    Core::Thread* pThread = (Thread*)uParam;

	pThread->Run();

	sys_ppu_thread_exit(0);
}
#endif

////////////////////////////////////////////////////////////////////////////////
//	Increment
////////////////////////////////////////////////////////////////////////////////
S32 Audio::Core::Thread::Increment(volatile S32* psCount)
{
#if defined(PLATFORM_PC)
	return InterlockedIncrement((volatile LONG*)psCount);
#else
	//if (0 != pthread_rwlock_wrlock(&m_lock))
	//	printf("Thread couldn't lock mutex");

	//(*psCount)++;

	//if (0 != pthread_rwlock_unlock(&m_lock))
	//	printf("Thread couldn't unlock mutex");
	return cellAtomicIncr32((std::uint32_t*)psCount);

	return *psCount;
#endif
}

////////////////////////////////////////////////////////////////////////////////
//	Increment
////////////////////////////////////////////////////////////////////////////////
S32 Audio::Core::Thread::Decrement(volatile S32* psCount)
{
#if defined(PLATFORM_PC)
	return InterlockedDecrement((volatile LONG*)psCount);
#else
	//if (0 != pthread_rwlock_wrlock(&m_lock))
	//	printf("Thread couldn't lock mutex");

	//(*psCount)--;

	//if (0 != pthread_rwlock_unlock(&m_lock))
	//	printf("Thread couldn't unlock mutex");
	return cellAtomicDecr32((std::uint32_t*)psCount);

	return *psCount;
#endif
}


#endif // [Blur2PC] native joined audio worker
