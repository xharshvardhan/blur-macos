// DWBS ------------------------------------------------------------------ DWBS
//
// PLATFORM		: UNIX OSX IPHONE
// PRODUCT		: BIT_DEMON
// VISIBILITY	: PRIVATE
//
// DWBS ------------------------------------------------------------------ DWBS


#include <bdPlatform/bdPlatformThread/bdPlatformSignal.h>
#include <bdPlatform/bdPlatformThread/bdPlatformMutex.h>
#include <stdio.h>

#if defined(BD_PLATFORM_OSX)

#include "bdNativeCountState.h"
// The original PC implementation is a Win32 automatic-reset event, retaining one pending signal.
bdSignalHandle BD_CALL bdPlatformSignal::create(){return bdNativeCountState::create(0,1);}
void BD_CALL bdPlatformSignal::destroy(bdSignalHandle& handle){bdNativeCountState::destroy(handle);}
void BD_CALL bdPlatformSignal::signal(bdSignalHandle& handle){bdNativeCountState::release(handle);}
bdBool BD_CALL bdPlatformSignal::wait(bdSignalHandle& handle){return bdNativeCountState::wait(handle);}

#else
bdSignalHandle BD_CALL bdPlatformSignal::create()
{
	bdSignalHandle handle;
	pthread_cond_init(&handle, BD_NULL);
	return handle;
}

void BD_CALL bdPlatformSignal::destroy(bdSignalHandle &handle)
{
	 pthread_cond_destroy(&handle);
}

void BD_CALL bdPlatformSignal::signal(bdSignalHandle &handle)
{
	pthread_cond_signal(&handle);	
}

bdBool BD_CALL bdPlatformSignal::wait(bdSignalHandle &handle)
{
	bdMutexHandle mutex;
	mutex = bdPlatformMutex::createMutex();
	bdPlatformMutex::lock(mutex);
	if (pthread_cond_wait(&handle, &mutex) != 0)
	{
		perror("pthread:");
		return false;
	}
	bdPlatformMutex::unlock(mutex);
	bdPlatformMutex::destroy(mutex);
	return true;
}



#endif // [Blur2PC] Native Darwin/original Unix implementation.
