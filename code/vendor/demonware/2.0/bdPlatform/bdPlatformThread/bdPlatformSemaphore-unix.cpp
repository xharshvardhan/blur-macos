// DWBS ------------------------------------------------------------------ DWBS
//
// PLATFORM		: UNIX OSX IPHONE
// PRODUCT		: BIT_DEMON
// VISIBILITY	: PUBLIC
//
// DWBS ------------------------------------------------------------------ DWBS

#include <bdPlatform/bdPlatformThread/bdPlatformSemaphore.h>
#include <bdPlatform/bdPlatformMemory/bdPlatformMemory.h>

#if defined(BD_PLATFORM_OSX)

#include "bdNativeCountState.h"
// Darwin sem_init returns ENOSYS. Use owned mutex/condition state with the original counting contract.
bdSemaphoreHandle BD_CALL bdPlatformSemaphore::createSemaphore(bdUInt initial,bdUInt maximum){return bdNativeCountState::create(initial,maximum);}
void BD_CALL bdPlatformSemaphore::destroy(bdSemaphoreHandle& handle){bdNativeCountState::destroy(handle);}
void BD_CALL bdPlatformSemaphore::release(bdSemaphoreHandle& handle){bdNativeCountState::release(handle);}
bdBool BD_CALL bdPlatformSemaphore::wait(bdSemaphoreHandle& handle){return bdNativeCountState::wait(handle);}

#else
bdSemaphoreHandle BD_CALL bdPlatformSemaphore::createSemaphore(bdUInt32 initialCount,
															   bdUInt32 maxCount)
{
	sem_t semaphore;
	bdMemset(&semaphore, 0, sizeof(sem_t));
	sem_init(&semaphore, 0, initialCount);
	return semaphore;
}

void BD_CALL bdPlatformSemaphore::destroy(bdSemaphoreHandle &handle)
{
	int res;
	res = sem_destroy(&handle);
}

void BD_CALL bdPlatformSemaphore::release(bdSemaphoreHandle &handle)
{
	sem_post(&handle);
}

bdBool BD_CALL bdPlatformSemaphore::wait(bdSemaphoreHandle &handle)
{
	sem_wait(&handle);
	return true;
}

#endif // [Blur2PC] Native Darwin/original Unix implementation.
