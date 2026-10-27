// [Blur2PC][macOS] Native reserve/commit/decommit/release, with real protections.
#include <system/memory/macos/NativeVirtualMemory.h>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

namespace Memory {
namespace {
// Match the engine BlockAllocator's 64 KiB page-base mask. Darwin's physical
// page can be smaller, so reserve extra space and trim to this alignment.
constexpr size_t AllocationGranularity = 64 * 1024;
struct Commit { uintptr_t begin; size_t bytes; Commit* next; };
struct Region { uintptr_t begin; size_t bytes; Commit* commits; Region* next; };
pthread_mutex_t mappingMutex = PTHREAD_MUTEX_INITIALIZER;
Region* regions = nullptr;
struct Guard {
    Guard() { pthread_mutex_lock(&mappingMutex); }
    ~Guard() { pthread_mutex_unlock(&mappingMutex); }
};
// Bookkeeping deliberately uses libc allocation, never operator new: the
// engine's global new can itself call BlockAllocator -> this mapping API.
template<class T> T* record() { return static_cast<T*>(std::calloc(1, sizeof(T))); }
bool rounded(U64 bytes, size_t alignment, size_t& result) {
    if (!bytes || bytes > std::numeric_limits<size_t>::max() - alignment) {
        errno = EINVAL;
        return false;
    }
    result = (static_cast<size_t>(bytes) + alignment - 1) & ~(alignment - 1);
    return true;
}
Region* containing(uintptr_t address) {
    for (auto* region = regions; region; region = region->next)
        if (address >= region->begin && address - region->begin < region->bytes) return region;
    return nullptr;
}
void clearCommits(Commit* list) {
    while (list) { auto* next = list->next; std::free(list); list = next; }
}
}

void* AllocVirtual(U64 requested) {
    size_t bytes;
    if (!rounded(requested, AllocationGranularity, bytes) ||
        bytes > std::numeric_limits<size_t>::max() - AllocationGranularity) {
        errno = EINVAL;
        return nullptr;
    }
    auto* region = record<Region>();
    if (!region) { errno = ENOMEM; return nullptr; }
    const size_t total = bytes + AllocationGranularity;
    void* raw = mmap(nullptr, total, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (raw == MAP_FAILED) { std::free(region); return nullptr; }
    const auto original = reinterpret_cast<uintptr_t>(raw);
    const auto base = (original + AllocationGranularity - 1) & ~(uintptr_t(AllocationGranularity) - 1);
    const size_t prefix = base - original;
    const size_t suffix = total - prefix - bytes;
    if ((prefix && munmap(raw, prefix)) ||
        (suffix && munmap(reinterpret_cast<void*>(base + bytes), suffix))) {
        const int error = errno;
        munmap(raw, total);
        std::free(region);
        errno = error;
        return nullptr;
    }
    region->begin = base;
    region->bytes = bytes;
    Guard guard;
    region->next = regions;
    regions = region;
    return reinterpret_cast<void*>(base);
}
void FreeVirtual(void* pointer) {
    if (!pointer) return;
    Guard guard;
    auto** link = &regions;
    const auto address = reinterpret_cast<uintptr_t>(pointer);
    while (*link && (*link)->begin != address) link = &(*link)->next;
    if (!*link) { errno = EINVAL; return; }
    auto* region = *link;
    if (munmap(pointer, region->bytes)) return;
    *link = region->next;
    clearCommits(region->commits);
    std::free(region);
}
void* AllocPhysical(void* pointer, U64 requested) {
    if (!requested) { errno = EINVAL; return nullptr; }
    if (!pointer) {
        void* reserved = AllocVirtual(requested);
        if (!reserved) return nullptr;
        void* committed = AllocPhysical(reserved, requested);
        if (!committed) { const int error = errno; FreeVirtual(reserved); errno = error; }
        return committed;
    }
    const size_t page = static_cast<size_t>(getpagesize());
    const auto address = reinterpret_cast<uintptr_t>(pointer);
    const auto start = address & ~(uintptr_t(page) - 1);
    if (requested > std::numeric_limits<U64>::max() - (address - start)) { errno = EINVAL; return nullptr; }
    size_t bytes;
    if (!rounded(requested + (address - start), page, bytes)) return nullptr;
    Guard guard;
    auto* region = containing(start);
    if (!region || bytes > region->bytes - (start - region->begin)) { errno = EINVAL; return nullptr; }
    auto* commit = record<Commit>();
    if (!commit) { errno = ENOMEM; return nullptr; }
    if (mprotect(reinterpret_cast<void*>(start), bytes, PROT_READ | PROT_WRITE)) {
        std::free(commit);
        return nullptr;
    }
    // Overlapping commits preserve their contents and become one tracked span.
    // Adjacent independent commits remain independently decommittable.
    auto lower = start;
    auto upper = start + bytes;
    bool expanded;
    do {
        expanded = false;
        for (auto* old = region->commits; old; old = old->next) {
            if (old->begin < upper && lower < old->begin + old->bytes) {
                if (old->begin < lower) { lower = old->begin; expanded = true; }
                if (old->begin + old->bytes > upper) { upper = old->begin + old->bytes; expanded = true; }
            }
        }
    } while (expanded);
    auto** link = &region->commits;
    while (*link) {
        auto* old = *link;
        if (old->begin < upper && lower < old->begin + old->bytes) {
            *link = old->next;
            std::free(old);
        } else link = &old->next;
    }
    commit->begin = lower;
    commit->bytes = upper - lower;
    commit->next = region->commits;
    region->commits = commit;
    return reinterpret_cast<void*>(start);
}
void FreePhysical(void* pointer) {
    if (!pointer) return;
    Guard guard;
    const auto address = reinterpret_cast<uintptr_t>(pointer);
    auto* region = containing(address);
    if (!region) { errno = EINVAL; return; }
    auto** link = &region->commits;
    while (*link && !(address >= (*link)->begin && address - (*link)->begin < (*link)->bytes))
        link = &(*link)->next;
    if (!*link) { errno = EINVAL; return; }
    auto* commit = *link;
    // Replace only the owned committed span with a fresh anonymous reservation.
    // This releases its physical pages and guarantees zero-filled recommit.
    if (mmap(reinterpret_cast<void*>(commit->begin), commit->bytes, PROT_NONE,
             MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED) return;
    *link = commit->next;
    std::free(commit);
}
bool AddressReadAccess(void* pointer) {
    if (!pointer) return false;
    mach_vm_address_t start = reinterpret_cast<uintptr_t>(pointer);
    const auto address = start;
    mach_vm_size_t bytes = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    const auto status = mach_vm_region(mach_task_self(), &start, &bytes, VM_REGION_BASIC_INFO_64,
        reinterpret_cast<vm_region_info_t>(&info), &count, &object);
    if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
    // This is a snapshot of mapping permissions, never a safe-dereference lock.
    return status == KERN_SUCCESS && address >= start && address - start < bytes &&
           (info.protection & VM_PROT_READ);
}
NativeVirtualStats nativeVirtualStats() {
    Guard guard;
    NativeVirtualStats stats{};
    for (auto* region = regions; region; region = region->next) {
        ++stats.regions;
        stats.reservedBytes += region->bytes;
        for (auto* commit = region->commits; commit; commit = commit->next)
            stats.committedBytes += commit->bytes;
    }
    return stats;
}
}
