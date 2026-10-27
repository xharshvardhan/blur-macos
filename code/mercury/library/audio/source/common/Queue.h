#ifndef _QUEUE_H_
#define _QUEUE_H_
#if defined(PLATFORM_MACOS) // [Blur2PC] Native queue producers and render consumer share actual synchronization.
#include <atomic>
#include <mutex>
#endif

#ifdef _RACERNOTAMAX
#include "Game\GameAccessControl.h"
#endif

#if defined (PLATFORM_PS3)
#include "pthread.h" // for pthread_mutex_t
#include <cell/atomic.h>
#include <ppu_intrinsics.h>
#endif // _PS3

//#define AUDIO_QUEUE_BUFFER_SIZE (16 * 1024)
//#define AUDIO_QUEUE_BUFFER_SIZE (384 * 1024)	//	must be power of 2

namespace Audio
{
	namespace Core
	{
////////////////////////////////////////////////////////////////////////////////
//	Queue
////////////////////////////////////////////////////////////////////////////////
        class Queue
        {
        public:
        public:
            Queue()
            {
#ifdef PLATFORM_PS3
                pthread_mutex_init(&m_Mutex, NULL);
#endif // _PS3

#ifdef PLATFORM_PC
				InitializeCriticalSection(&m_CriticalSection);
#endif

				m_sLock = -1;
            	m_sReadOffset = 0;
	            m_sWriteOffset = 0;
				m_uNumBytesQueued = 0;
				m_uPeakBytesQueued = 0;
				m_paBuffer = 0;
				m_uQueueArraySize = 0;
				m_sInterval = 0;
            }
            ~Queue()
            {
				delete [] m_paBuffer;
#ifdef PLATFORM_PC
				DeleteCriticalSection(&m_CriticalSection);
#endif

#ifdef PLATFORM_PS3
                pthread_mutex_destroy(&m_Mutex);
#endif // _PS3
            }
			
			void Init(U32 uNumQueueBytes);

			bool SendItem(U32 eItem, const void* pData, U32 uSize);
			virtual bool ReceiveItem() = 0;

            void Increment(S32* psThis, S32 sBy)
            {
#if defined(PLATFORM_MACOS) // [Blur2PC] Preserve atomic counter updates.
                __atomic_add_fetch(psThis,sBy,__ATOMIC_SEQ_CST);
#endif
#if defined(PLATFORM_PC)
                InterlockedExchangeAdd((LONG*)psThis, sBy);
#endif // _XBOX

#if defined (PLATFORM_PS3)
#if 0
                pthread_mutex_lock(&m_Mutex);
                *psThis += sBy;
                pthread_mutex_unlock(&m_Mutex);
#else
				cellAtomicAdd32( (std::uint32_t *)psThis, sBy );
#endif
#endif // _PS3
            };

#if defined(PLATFORM_MACOS) // [Blur2PC] Native consumer offsets publish reclaimed storage atomically.
            void Increment(std::atomic<S32>* counter,S32 amount){counter->fetch_add(amount);}
#endif
			void AcquireLock(void)
			{
#if defined(PLATFORM_MACOS) // [Blur2PC] Serialize original multi-producer message publication.
                m_NativeMutex.lock();
#endif

#ifdef PLATFORM_PC
				EnterCriticalSection(&m_CriticalSection);
#endif

#if defined (PLATFORM_PS3)
#if 1
				#define  ICompareExchange( x, y, z ) (S32)cellAtomicCompareAndSwap32( (std::uint32_t*)x, z, y )
				for (;;)
				{
					// *pLock is -1 when available, 0 when locked.
					if (-1 == ICompareExchange(&m_sLock, 0, -1))
					{
						// At this point, you have safely acquired the lock. However, if you do not have an import
						// barrier, then subsequent reads might get stale copies of data that has been updated in L2.
						__lwsync();
						return;
					}
				}

#else
				for (;;)
				{
					pthread_mutex_lock(&m_Mutex);
					if (m_sLock == -1)
					{
						m_sLock = 0;
						pthread_mutex_unlock(&m_Mutex);
						return;
					}
					pthread_mutex_unlock(&m_Mutex);
				}
#endif
#endif // _PS3
			}

			void ReleaseLock(void)
			{
#if defined(PLATFORM_MACOS) // [Blur2PC] Pair every producer lock without unsynchronized legacy lock writes.
                m_sLock=-1;m_NativeMutex.unlock();return;
#endif

#ifdef PLATFORM_PS3
				__lwsync();
#endif

#ifdef PLATFORM_PC
				LeaveCriticalSection(&m_CriticalSection);
#endif

				// m_sLock is -1 when available, 0 when locked.
				m_sLock = -1;
			}

			void Update();

			U32 PeakQueuedBytes() { return m_uPeakBytesQueued; }
			U32 QueuedBytes() {
#if defined(PLATFORM_MACOS) // [Blur2PC] The UI samples statistics concurrently with rendering.
                return m_uNumBytesQueued.exchange(0);
#else
                U32 retVal=m_uNumBytesQueued;m_uNumBytesQueued=0;return retVal;
#endif
            }

			void ResetPeakQueuedBytes() { m_uPeakBytesQueued = 0; }
			
		protected:
#if defined (PLATFORM_PS3)
            pthread_mutex_t m_Mutex;
#endif // _PS3

#ifdef PLATFORM_PC
			CRITICAL_SECTION	m_CriticalSection;
#endif

            U32* m_paBuffer;
			U32 m_uQueueArraySize;

			// TODO: Need these to be on different cache lines
			volatile S32 m_sLock __attribute__((aligned(128)));
			S32 padding[31];
			#if defined(PLATFORM_MACOS) // [Blur2PC] Publish completed packets and consumed storage across native threads.
            std::mutex m_NativeMutex;
            std::atomic<S32> m_sReadOffset;
            std::atomic<S32> m_sWriteOffset;
#else
            S32 m_sReadOffset;
            S32 m_sWriteOffset;
#endif

			S32 m_sInterval;

			#if defined(PLATFORM_MACOS) // [Blur2PC] Renderer statistics are read/reset by the UI thread.
            std::atomic<U32> m_uNumBytesQueued;
            std::atomic<U32> m_uPeakBytesQueued;
#else
            U32 m_uNumBytesQueued; // num bytes currently in queue
            U32 m_uPeakBytesQueued; // maximum number of bytes ever queued up
#endif
        };


		class SendQueue : public Queue
		{

		public:
			enum SendQueueItem
			{
				eNoItem = 0,
				eParameterIn,
				eVectorIn,
				eActionIn,
				eObjectManager,
				eSetMaster,
				eSetMonoMaster,
				eSetStereoMaster,
				eSetSurroundMaster,
				eSetSurroundLFEMaster,

				eNumItems
			};

			bool ReceiveItem();
		};


		class ReturnQueue : public Queue
		{

		public:
			enum ReturnQueueItem
			{
				eNoItem = 0,
				eGetValue,
				eActionOut,
				eDestroyObject,

				eNumItems
			};

			bool ReceiveItem();
		};
	}
}


#endif // _QUEUE_H_

