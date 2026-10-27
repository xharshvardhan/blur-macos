#ifndef SCENEGRAPH_MAX_CONTEXTS_H
#define SCENEGRAPH_MAX_CONTEXTS_H

#ifndef SCENEGRAPH_MAX_CONTEXTS
#if defined(PLATFORM_MACOS)
// [Blur2PC] Native jobqueue_xenon.cpp uses main slot0 and up to seven workers, slots1..7.
#define SCENEGRAPH_MAX_CONTEXTS 8
#else
#define SCENEGRAPH_MAX_CONTEXTS 7
#endif
#endif


#if defined(PLATFORM_MACOS)
#include <stdexcept>
namespace SceneGraph {
// [Blur2PC] Reject invalid logical IDs in every build; never alias another worker context.
inline int CheckedContextIndex(int index)
{
    if(index < 0 || index >= SCENEGRAPH_MAX_CONTEXTS)
        throw std::out_of_range("Native scenegraph worker context ID is outside slots0..7");
    return index;
}
}
#endif

#endif
