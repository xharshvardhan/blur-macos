// DWBS ------------------------------------------------------------------ DWBS
//
// PLATFORM		: UNIX OSX IPHONE
// PRODUCT		: BIT_DEMON
// VISIBILITY	: PUBLIC
//
// DWBS ------------------------------------------------------------------ DWBS

#include <bdPlatform/bdPlatformTiming/bdPlatformTiming.h>

#include <sys/time.h>
#include <unistd.h>

#if defined(BD_PLATFORM_OSX)

#include <time.h>
#include <errno.h>
bdUInt64 BD_CALL bdPlatformTiming::getHiResTimeStamp(){
 timespec time{};clock_gettime(CLOCK_MONOTONIC,&time);
 return static_cast<bdUInt64>(time.tv_sec)*1000000u+static_cast<bdUInt64>(time.tv_nsec)/1000u;
}
bdFloat32 BD_CALL bdPlatformTiming::getElapsedTime(bdUInt64 first,bdUInt64 last){return static_cast<bdFloat32>(last-first)/1000000.0f;}
void BD_CALL bdPlatformTiming::sleep(bdUInt32 milliseconds){
 timespec delay{static_cast<time_t>(milliseconds/1000u),static_cast<long>(milliseconds%1000u)*1000000L};
 while(nanosleep(&delay,&delay)<0 && errno==EINTR){}
}
bdUInt32 BD_CALL bdPlatformTiming::getLoResTimeStamp(){return static_cast<bdUInt32>(getHiResTimeStamp()/1000u);}
bdUInt32 BD_CALL bdPlatformTiming::getLoResElapsedTime(bdUInt32 first,bdUInt32 last){return (last-first)/1000u;}

#else
bdUInt64 BD_CALL bdPlatformTiming::getHiResTimeStamp()
{
	struct timeval tv;
	// returns time in seconds and microseconds
	gettimeofday(&tv, 0);

	bdUInt64 time;

	// multiply seconds by 10^6
	time = static_cast<bdUInt64>(tv.tv_sec) * 1000000;
	// add on microseconds
	time += tv.tv_usec;

	return time;
}

bdFloat32 BD_CALL bdPlatformTiming::getElapsedTime(const bdUInt64 time1, const bdUInt64 time2)
{
	const bdUInt64 elapsedTime = time2 - time1;

	return static_cast<bdFloat32>(elapsedTime) / 1000000;
}

void BD_CALL bdPlatformTiming::sleep(bdUInt32 timeInMs)
{
	usleep(timeInMs * 1000);
}

bdUInt32 BD_CALL bdPlatformTiming::getLoResTimeStamp()
{
	struct timeval tv;
	gettimeofday(&tv, 0);

	const bdUInt32 msTimestamp = ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
	return msTimestamp ;
}

bdUInt32 BD_CALL bdPlatformTiming::getLoResElapsedTime(const bdUInt32 time1,
													   const bdUInt32 time2)
{
	return ((time2 - time1) / 1000);
}

#endif // [Blur2PC] Native Darwin/original Unix implementation.
