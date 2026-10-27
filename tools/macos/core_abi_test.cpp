// [Blur2PC][macOS] Check the real engine's platform identity and serialized ABI.
#include <cstddef>
#include <cstdint>
#include <core/Core.h>
#include <pakfs/impl/PakHeader.h>

#ifndef PLATFORM_MACOS
#error Apple hosts must have a distinct native platform identity
#endif
#ifdef PLATFORM_WINDOWS
#error Native builds must not masquerade as Win32
#endif
#ifdef PLATFORM_LINUX
#error Darwin must not use the legacy 32-bit Linux fallback
#endif
static_assert(WORD_SIZE == 64);
static_assert(sizeof(INTPTR) == sizeof(void*));
static_assert(sizeof(DWORD) == 4);
static_assert(sizeof(U8) == 1 && sizeof(U16) == 2 && sizeof(U32) == 4 && sizeof(U64) == 8);
static_assert(sizeof(S8) == 1 && sizeof(S16) == 2 && sizeof(S32) == 4 && sizeof(S64) == 8);
static_assert(sizeof(Pak::C_PreHeader) == 12);
static_assert(sizeof(Pak::C_PakHeaderV2) == 32);
static_assert(sizeof(Pak::C_PakFatEntryV2) == 40);

int main() {
    // A high address must survive the actual core pointer-integer typedef.
    auto value = static_cast<INTPTR>(std::uint64_t{0x123456789abcdef0});
    return static_cast<std::uint64_t>(value) == 0x123456789abcdef0 ? 0 : 1;
}
