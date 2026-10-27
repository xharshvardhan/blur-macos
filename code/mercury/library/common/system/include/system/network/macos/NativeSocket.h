// [Blur2PC][macOS] Darwin implementation details for the existing Socket API.
#pragma once
#include <core/Core.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace Network { namespace NativeMac {
inline void CheckCreated(int descriptor) {
    if (descriptor < 0) throw std::system_error(errno, std::generic_category(), "socket");
    int enabled = 1;
    if (setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) ||
        fcntl(descriptor, F_SETFD, FD_CLOEXEC) == -1) {
        const int error = errno;
        close(descriptor);
        throw std::system_error(error, std::generic_category(), "socket options");
    }
}
inline bool InputPending(int descriptor, U32 milliseconds) {
    if (descriptor < 0) return false;
    const bool infinite = milliseconds == U32(-1);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    for (;;) {
        int timeout = -1;
        if (!infinite) {
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            timeout = static_cast<int>(std::clamp<int64_t>(remaining, 0, INT_MAX));
        }
        pollfd pending{descriptor, POLLIN, 0};
        const int result = poll(&pending, 1, timeout);
        if (result > 0) return pending.revents & (POLLIN | POLLHUP | POLLERR);
        if (result < 0 && errno != EINTR) return false;
        if (!infinite && std::chrono::steady_clock::now() >= deadline) return false;
    }
}
inline U32 Transfer(int descriptor, const void* buffer, U64 length, int flags, bool sending) {
    // U32(-1) remains the original error sentinel; cap a single transfer below
    // it so a successful count can never be mistaken for an error.
    const size_t amount = static_cast<size_t>(std::min<U64>(length, std::numeric_limits<U32>::max() - 1ULL));
    ssize_t result;
    do {
        result = sending ? send(descriptor, buffer, amount, flags)
                         : recv(descriptor, const_cast<void*>(buffer), amount, flags);
    } while (result < 0 && errno == EINTR);
    return result < 0 ? U32(-1) : static_cast<U32>(result);
}
}}
