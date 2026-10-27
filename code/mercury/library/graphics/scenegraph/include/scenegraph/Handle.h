#ifndef SCENE_GRAPH_HANDLE_H
#define SCENE_GRAPH_HANDLE_H

#ifdef _MSC_VER
#pragma once
#endif

#if defined(PLATFORM_MACOS)
#include <memory>
#endif

namespace SceneGraph
{
#if defined(PLATFORM_MACOS)
// [Blur2PC][macOS] Transient ownership marker. A handle may outlive a pooled
// object, so its validity must be checked without reading the destroyed object.
struct C_HandleLifetime {};
class C_HandleLifetimeOwner
{
public:
    C_HandleLifetimeOwner() : m_Token(std::make_shared<C_HandleLifetime>()) {}
    // [Blur2PC][macOS] Copies are distinct objects; assigning fields to an
    // existing object must preserve that destination object's lifetime.
    C_HandleLifetimeOwner(const C_HandleLifetimeOwner&) : C_HandleLifetimeOwner() {}
    C_HandleLifetimeOwner& operator=(const C_HandleLifetimeOwner&) { return *this; }
    std::weak_ptr<C_HandleLifetime> Weak() const { return m_Token; }
private:
    std::shared_ptr<C_HandleLifetime> m_Token;
};
#endif

template <typename T>
class C_Handle
{
public:

	C_Handle() : m_pObject(0), m_Salt(0) {}
	C_Handle(T* p) { m_pObject = p; m_Salt = p->m_Salt;
#if defined(PLATFORM_MACOS)
        m_Lifetime = p->m_HandleLifetime.Weak(); // [Blur2PC][macOS] A new allocation has a new owner.
#endif
    }
	~C_Handle() {};

public:

	INLINE T* Object()
	{
		#if defined(PLATFORM_MACOS)
        // [Blur2PC][macOS] The free-list overlays the former object's vptr.
        // Check the independent lifetime before any access through m_pObject.
        if (m_Lifetime.expired())
            return 0;
#endif
        if (!m_pObject || (m_pObject->m_Salt != m_Salt))
			return 0;

		return m_pObject;
	}

	INLINE const T* Object() const
	{
		#if defined(PLATFORM_MACOS)
        // [Blur2PC][macOS] The free-list overlays the former object's vptr.
        // Check the independent lifetime before any access through m_pObject.
        if (m_Lifetime.expired())
            return 0;
#endif
        if (!m_pObject || (m_pObject->m_Salt != m_Salt))
			return 0;

		return m_pObject;
	}

	inline operator T*()					{ return Object(); }
	inline operator const T*() const		{ return Object(); }
	inline operator T&()					{ T*		object = Object(); assert(object != 0); return *object;	}
	inline operator const T&() const		{ const T*	object = Object(); assert(object != 0); return *object;	}
	inline T* operator->()					{ T*		object = Object(); assert(object != 0); return object;	}
	inline const T* operator->() const		{ const T*	object = Object(); assert(object != 0); return object;	}

protected:

	T*	m_pObject;
	U32	m_Salt;
#if defined(PLATFORM_MACOS)
    std::weak_ptr<C_HandleLifetime> m_Lifetime; // [Blur2PC][macOS] Runtime-only, never serialized.
#endif

};

} // namespace SceneGraph

#endif // SCENE_GRAPH_HANDLE_H
