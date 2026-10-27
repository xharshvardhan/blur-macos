// DWBS ------------------------------------------------------------------ DWBS
//
// PLATFORM		: UNIX OSX IPHONE
// PRODUCT		: BIT_DEMON
// VISIBILITY	: PUBLIC
//
// DWBS ------------------------------------------------------------------ DWBS

#include <bdPlatform/bdPlatformThread/bdPlatformMutex.h>

#if defined(BD_PLATFORM_OSX)

#include <new>
#include <system_error>
bdMutexHandle BD_CALL bdPlatformMutex::createMutex(){
 auto* mutex=new(std::nothrow) pthread_mutex_t;
 if(!mutex)return nullptr;
 if(pthread_mutex_init(mutex,nullptr)!=0){delete mutex;return nullptr;}return mutex;
}
void BD_CALL bdPlatformMutex::lock(bdMutexHandle& handle){
 if(!handle)throw std::system_error(EINVAL,std::generic_category(),"Demonware mutex");
 const int error=pthread_mutex_lock(handle);if(error)throw std::system_error(error,std::generic_category(),"Demonware mutex lock");
}
void BD_CALL bdPlatformMutex::unlock(bdMutexHandle& handle){
 if(!handle)throw std::system_error(EINVAL,std::generic_category(),"Demonware mutex");
 const int error=pthread_mutex_unlock(handle);if(error)throw std::system_error(error,std::generic_category(),"Demonware mutex unlock");
}
void BD_CALL bdPlatformMutex::destroy(bdMutexHandle& handle){if(handle){pthread_mutex_destroy(handle);delete handle;handle=nullptr;}}

#else
bdMutexHandle BD_CALL bdPlatformMutex::createMutex()
{
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, 0);
	return mutex;
}

void BD_CALL bdPlatformMutex::lock(bdMutexHandle &handle)
{
	pthread_mutex_lock(&handle);
}

void BD_CALL bdPlatformMutex::unlock(bdMutexHandle &handle)
{
	pthread_mutex_unlock(&handle);
}

void BD_CALL bdPlatformMutex::destroy(bdMutexHandle &handle)
{
	pthread_mutex_destroy(&handle);
}


#endif // [Blur2PC] Native Darwin/original Unix implementation.
