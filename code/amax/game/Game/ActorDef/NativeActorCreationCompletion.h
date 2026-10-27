#ifndef AMAX_NATIVE_ACTOR_CREATION_COMPLETION_H
#define AMAX_NATIVE_ACTOR_CREATION_COMPLETION_H
#if defined(PLATFORM_MACOS)
namespace Game {
// [Blur2PC] Runtime-only side interface: retirement/cancellation is not creation success.
// The original Actor::I_ActorCreationList ABI and per-actor Continue policy remain intact.
class I_NativeActorCreationCompletion
{
public:
    virtual ~I_NativeActorCreationCompletion() = default;
    virtual void NativeActorCreationSucceeded() = 0;
};
}
#endif
#endif
