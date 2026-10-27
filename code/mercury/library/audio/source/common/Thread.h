#ifndef _THREAD_H_
#define _THREAD_H_

#if defined (PLATFORM_PS3)
#include <pthread.h>
#endif	//	PLATFORM_PS3

#include "Util.h"
#include "../core/audio_core.h"
#include "types.h"
#if defined(PLATFORM_MACOS) // [Blur2PC] Native worker ownership is independent of the unity PCH.
#include <thread>
#include <atomic>
#include <chrono>
#endif

namespace Audio
{
    namespace Core
	{
////////////////////////////////////////////////////////////////////////////////
//	Thread
////////////////////////////////////////////////////////////////////////////////
		class Thread
		{
		public:
			enum Priority
			{
#if defined(PLATFORM_PC)
				eIdle = THREAD_PRIORITY_IDLE,
				eLowest = THREAD_PRIORITY_LOWEST,
				eBelowNormal = THREAD_PRIORITY_BELOW_NORMAL,
				eNormal = THREAD_PRIORITY_NORMAL,
				eAboveNormal = THREAD_PRIORITY_ABOVE_NORMAL,
				eHighest = THREAD_PRIORITY_HIGHEST,
				eTimeCritical = THREAD_PRIORITY_TIME_CRITICAL
#else
				eIdle = 3000,
				eLowest = 2000,
				eBelowNormal = 1500,
				eNormal = 1001,
				eAboveNormal = 750,
				eHighest = 500,
				eTimeCritical = 10
#endif
			};


		public:
			Thread();
			Thread(U32 uStackSize);
			audio_virtual ~Thread();
			Thread(U32 uStackSize, Priority ePriority);

			bool Start(void* pContext);
#if defined (PLATFORM_PC)
			S32 Stop();
#endif	//	PLATFORM_PC
			S32 Suspend();
			S32 Resume();
			S32 WaitUntilTerminated();
			void SleepThread(U32 uMillisecs)
			{
#if defined(PLATFORM_PC)
				::Sleep(uMillisecs);
#elif defined(PLATFORM_MACOS)
				// [Blur2PC] Native elapsed-time sleep, independent of rendering.
				std::this_thread::sleep_for(std::chrono::milliseconds(uMillisecs));
#else
				sys_timer_usleep(uMillisecs * 1000);
#endif
			}

			const U32 GetExitCode() const { return m_uExitCode; }

			bool SetPriority(Priority ePriority);

		protected:
			S32 Run();
#if defined(PLATFORM_PC)
			static S32 EntryPoint(void*);
#elif defined(PLATFORM_MACOS)
			// [Blur2PC] std::thread owns the native audio worker.
			static void EntryPoint(Thread*);
#else //PS3
            static void EntryPoint(uint64_t uParam);
#endif

			audio_virtual void Setup() {}
			audio_virtual void Execute(void*) = 0;
			audio_virtual S32 Shutdown() { return 0; }

			S32 Abort();

			const U32 StackSize() const { return m_uStackSize; }

#if defined(PLATFORM_MACOS) // [Blur2PC] Native renderer ring counters have atomic storage.
            S32 Increment(std::atomic<S32>* count){return count->fetch_add(1)+1;}
            S32 Decrement(std::atomic<S32>* count){return count->fetch_sub(1)-1;}
#endif
            S32 Increment(volatile S32* psCount);
			S32 Decrement(volatile S32* psCount);

		private:
#if defined(PLATFORM_PC)
			HANDLE m_hThread;
#elif defined(PLATFORM_MACOS)
			// [Blur2PC] No console thread handles on Darwin.
			std::thread m_NativeThread;
#else
            sys_ppu_thread_t m_hThread;
			pthread_rwlock_t m_lock;
#endif // PLATFORM_XENON / PLATFORM_PC
			void* m_pThreadContext;
			U32 m_uStackSize;
			Priority m_ePriority;
			U32 m_uExitCode;
		};
	};
};

#endif // _THREAD_H_

