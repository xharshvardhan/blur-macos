#ifndef _LOCK_H_
#define _LOCK_H_

#if defined (PLATFORM_PS3)
#include "pthread.h" // for pthread_mutex_t
#endif // _PS3 

#include "core/threads/Mutex.h"

namespace Audio
{
	namespace Core
	{
////////////////////////////////////////////////////////////////////////////////
//	Lock
////////////////////////////////////////////////////////////////////////////////
		class Lock : public MT::Lock
        {
        public:
			bool Acquire()
			{
				Enter();
				return true;
			}
			bool Release()
			{
				Exit();
				return true;
			}

            void Increment(volatile S32* psThis, S32 sBy)
            {
				MT::AtomicAdd((U32*)psThis, sBy);
            };
        };

		class ThreadSafeIndex
		{
		public:
			ThreadSafeIndex() { m_u32Index = 0; }
			ThreadSafeIndex(U32 u32InitValue) { m_u32Index = u32InitValue; }

			void operator =(const U32 u32)
			{
#if defined(PLATFORM_PC)
				InterlockedExchange((LONG*)(&m_u32Index), u32);
#elif defined(PLATFORM_MACOS) // [Blur2PC] Publish actual native queue indexes.
                __atomic_store_n(&m_u32Index,u32,__ATOMIC_SEQ_CST);
#elif defined(PLATFORM_PS3)
				cellAtomicStore32((uint32_t*)&m_u32Index, u32);
#endif
			}

			void operator +=(const U32 u32)
			{
#if defined(PLATFORM_PC)
				InterlockedExchangeAdd((LONG*)&m_u32Index, u32);
#elif defined(PLATFORM_MACOS) // [Blur2PC] Publish actual native queue indexes.
                __atomic_fetch_add(&m_u32Index,u32,__ATOMIC_SEQ_CST);
#elif defined(PLATFORM_PS3)
				cellAtomicAdd32((uint32_t*)&m_u32Index, u32);
#endif
			}

//			const U32 operator +(const U32& u32) { return u32 + m_u32Index; }
			operator U32() {
#if defined(PLATFORM_MACOS) // [Blur2PC] Match native queue stores.
                return __atomic_load_n(&m_u32Index,__ATOMIC_SEQ_CST);
#else
                return m_u32Index;
#endif
            }

		private:
			volatile U32	m_u32Index;
		};
    }
}

#endif // _LOCK_H_

