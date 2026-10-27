// DWBS ------------------------------------------------------------------ DWBS
//
// PLATFORM		: UNIX OSX IPHONE
// PRODUCT		: BIT_DEMON
// VISIBILITY	: PUBLIC
//
// DWBS ------------------------------------------------------------------ DWBS


#include <bdPlatform/bdPlatformRandom/bdPlatformTrulyRandom.h>

#include <fcntl.h>
#include <unistd.h>

#if defined(BD_PLATFORM_OSX)

#include <stdlib.h>
void BD_CALL bdGetRandomUChar8(bdUByte8 output[],bdUInt32 length){arc4random_buf(output,length);}

#else
// The handle to the random device.
static bdInt32 s_fd;

void BD_CALL bdGetRandomUChar8(bdUByte8 in[], const bdUInt32 length)
{
    if (s_fd == 0)
    {
            s_fd = open("/dev/urandom",O_RDONLY);
    }
    if (read(s_fd, in, length) == -1)
    {
            close(s_fd);
            s_fd = open("/dev/urandom",O_RDONLY);
            read(s_fd, in, length);
    }
}


#endif // [Blur2PC] Native Darwin/original Unix implementation.
