#ifndef BTL_POOL_H
#define BTL_POOL_H
#ifdef _MSC_VER
#pragma once
#endif

///////////////////////////////////////////////////////
// Pool
///////////////////////////////////////////////////////

// Allocate fixed size objects from pool.

#include <core/utility/coredebug.h>
#include "btl/utility/threads.h"
#include "btl/containers/list.h"
#include "btl/containers/vector.h"
#include "btl/utility/types.h"
#include "btl/allocators/allocator.h"

namespace Btl
{
#if defined(PLATFORM_MACOS)
// [Blur2PC] A free slot contains the intrusive link, while an allocated slot
// contains T. Their alignment and size requirements can differ on LP64.
// This changes only allocator storage; the layout of T and serialized data stays
// unchanged. The byte array provides storage for either object's lifetime.
template<class T> struct NativePoolStorage
{
    struct ListItem:ListElementSingle<ListItem> {void Destroy(){}};
    struct alignas(T) alignas(ListItem) Slot
    {
        U8 bytes[StaticMax<(int)sizeof(T),(int)sizeof(ListItem)>::RESULT];
    };
};
template<class T> struct PoolTypes;
#endif
////////////////////////////////////////////////////////

/// @addtogroup Allocators
/// @{

/// Pre-allocated pool of objects of type T.
/// @param T The type of object in the pool.
/// @param N The number of pre-allocated objects. 
///	@remarks T must be at least the size of a pointer.
template <class T, int N>
class StaticPool
{
public:
	StaticPool();
	~StaticPool() {assert(m_list.Size() == N);}

	T* Allocate(); ///< Allocate an object.
	void Free(T* p); ///< Free an object.

	bool Empty() {return m_list.Empty();} ///< Test for empty pool.

	/// The number of allocated objects 
	/// @remarks Slow since the pool does not maintain a count.
	int Allocated() const {return N - m_list.Size();}

	/// The number of objects left in the pool.
	/// @remarks Slow since the pool does not maintain a count.
	int Remaining() const {return m_list.Size();}

	/// This gives a pool type which allocates U instead of T.
	template <typename U> struct Rebind {typedef StaticPool<U, N> Other;};

protected:

#if defined(PLATFORM_MACOS)
    typedef typename NativePoolStorage<T>::Slot Slot;
    typedef typename NativePoolStorage<T>::ListItem ListItem;
    Vector<Slot, VectorParameters_Array<N> > m_pool;
#else
	Vector<SameSizeAndAlignment<T>, VectorParameters_Array<N> > m_pool;

	// dummy list item type
	struct ListItem : public ListElementSingle<ListItem> {void Destroy() {}};
#endif

	List<ListItem, ListParameters<SingleEndedList, OnDemandCounter> > m_list;

private:
	StaticPool(StaticPool& other); // copy protection
	void operator = (StaticPool& other); // copy protection
};

/// @}

//////////////////////////////////////////////////////////
// inline functions

template <class T, int N>
StaticPool<T, N>::StaticPool()
{
#if defined(PLATFORM_MACOS)
    static_assert(sizeof(Slot)>=sizeof(ListItem) && alignof(Slot)>=alignof(ListItem),"native pool free-list storage");
#else
	BIZ_STATIC_ASSERT(sizeof(T) >= sizeof(ListItem)); // must be space for a ListItem inside a T
#endif

	for (int i = 0; i < N; i++)
	{
#if defined(PLATFORM_MACOS)
        m_list.PushFront(*new(m_pool[i].bytes) ListItem);
#else
		m_list.PushFront((ListItem&) m_pool[i]);
#endif
	}
}

template <class T, int N>
inline T* StaticPool<T, N>::Allocate()
{
	assert(!m_list.Empty()); // error to call Allocate() when no items remaining

	T* p = (T*) &m_list.Front();
	m_list.PopFront();

	// Invoke default constructor on allocated object - this is essential if T has virtual functions.
	new(p) T;

	return p;
}

template <class T, int N>
inline void StaticPool<T, N>::Free(T* p)
{
	if (p == 0) return;

#if defined(PLATFORM_MACOS)
    assert(m_pool.Validate(reinterpret_cast<Slot*>(p)));
#else
	assert(m_pool.Validate((SameSizeAndAlignment<T>*)p)); // check that the pointer came from the pool
#endif

	// Call object's destructor.
	p->T::~T();

#if defined(PLATFORM_MACOS)
    m_list.PushFront(*new(p) ListItem);
#else
	m_list.PushFront((ListItem&) *p);
#endif
}

/////////////////////////////////////////////////////////////
// DynamicPool
/////////////////////////////////////////////////////////////

/// @addtogroup Allocators
/// @{

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4348)
#endif

/// Dynamic pool of objects of type T.
/// When the pool is empty a new block is added automatically.
/// @param T The type of object allocated by the pool.
/// @param AllocatorType The allocator used to create memory blocks.
/// @param ThreadingModel See @ref Threading_Models.
// todo: clean-up function to free unused blocks
// default parameters in forward definition (list.h)
template <class T, typename AllocatorType, class ThreadingModel>
class DynamicPool
{
public:
	DynamicPool();
	~DynamicPool();

	T* Allocate(); ///< Allocate an object.
	T* AllocateNoConstructor(); ///< Allocate an object without calling it's constructor.
	T* AllocatePrefetch(); ///< Allocate an object and prefetch next object.
	void Prefetch(); ///< Prefetch next object in anticipation of an Allocate.
	void Free(T* p); ///< Free an object.
	

	/// This gives a pool type which allocates U instead of T.
	template <typename U> struct Rebind {typedef DynamicPool<U, AllocatorType, ThreadingModel> Other;};

	friend void Swap(DynamicPool& left, DynamicPool& right) {SwapByMemcopy(left, right);}

	/// The total number of bytes used by the pool
	int SizeInBytes() const {return m_sizeInBytes;}

	/// The number of objects allocated by the pool.
	/// @remarks Slow since the pool does not maintain a count.
	int Allocated() const {
#if defined(PLATFORM_MACOS)
        return m_pool.Size()*PoolTypes<T>::N-Remaining();
#else
        return m_sizeInBytes / sizeof(T) - Remaining();
#endif
    }

	/// The number of objects that can be allocated before the size of the pool will be increased.
	/// @remarks Slow since the pool does not maintain a count.
	int Remaining() const {return m_list.Size();}

private:
	void Grow();

	int m_sizeInBytes;

	// hold pointers to blocks
	Vector<U8*> m_pool; // deque may be better if the number of blocks is large

	// dummy list item type
#if defined(PLATFORM_MACOS)
    typedef typename NativePoolStorage<T>::ListItem ListItem;
#else
	struct ListItem : public ListElementSingle<ListItem> {void Destroy() {}};
#endif

	List<ListItem, ListParameters<SingleEndedList, OnDemandCounter> > m_list;

	ThreadingModel m_threadData;

	DynamicPool(DynamicPool& other); // copy protection
	void operator = (DynamicPool& other); // copy protection
};

template <class T, typename AllocatorType, class ThreadingModel>
void Swap(DynamicPool<T, AllocatorType, ThreadingModel>& left, DynamicPool<T, AllocatorType, ThreadingModel>& right) 
{
	SwapByMemcopy(left, right);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/// @}

/////////////////////////////////////////////////////////////////////

// Separate class to Pool so that you can declare Pool<T> before defining T.
template <class T>
struct PoolTypes
{
#if defined(PLATFORM_MACOS)
    typedef typename NativePoolStorage<T>::Slot Slot;
    const static int N=StaticMax<4*1024/(int)sizeof(Slot),1>::RESULT;
    typedef Vector<Slot,VectorParameters_Array<N> > Block;
    // AllocatorType supplies bytes, so it need not know the block's alignment.
    // Retain its raw pointer for Free while constructing the block within it.
    const static int AllocationBytes=sizeof(Block)+alignof(Block)-1;
    static Block* BlockAt(U8* raw) {return reinterpret_cast<Block*>(Btl::AlignUp(raw,(INTPTR)alignof(Block)));}
#else
	const static int N = StaticMax<4 * 1024 / (int)sizeof(T), 1>::RESULT;
	typedef Vector<SameSizeAndAlignment<T>, VectorParameters_Array<N> > Block;
#endif
};

template <class T, typename AllocatorType, class ThreadingModel>
inline DynamicPool<T, AllocatorType, ThreadingModel>::DynamicPool() : m_sizeInBytes(0)
{
#if defined(PLATFORM_MACOS)
    static_assert(sizeof(typename PoolTypes<T>::Slot)>=sizeof(ListItem),"native pool free-list storage");
#else
	BIZ_STATIC_ASSERT(sizeof(T) >= sizeof(ListItem)); // must be space for a ListItem inside a T
#endif
}

template <class T, typename AllocatorType, class ThreadingModel>
DynamicPool<T, AllocatorType, ThreadingModel>::~DynamicPool()
{
	BIZ_ASSERT_MSG(m_list.Size() == PoolTypes<T>::N * m_pool.Size(), "m_list.Size(%d) PoolTypes<T>::N(%d) m_pool.Size(%d)", m_list.Size(), PoolTypes<T>::N, m_pool.Size());

	m_list.Abandon(); // stop list clearing

	while (!m_pool.Empty())
	{
		typedef typename PoolTypes<T>::Block U;
#if defined(PLATFORM_MACOS)
        PoolTypes<T>::BlockAt(m_pool.Back())->U::~U();
        AllocatorType::Free(m_pool.Back(),PoolTypes<T>::AllocationBytes);
#else
		((U*) m_pool.Back())->U::~U();
		AllocatorType::Free(m_pool.Back(), sizeof(U));
#endif
		m_pool.PopBack();
	}
}

template <class T, typename AllocatorType,  class ThreadingModel>
inline T* DynamicPool<T, AllocatorType, ThreadingModel>::Allocate()
{
	typename ThreadingModel::Lock lock(m_threadData);

	if (m_list.Empty())
		Grow();

	if (m_list.Empty())
		return 0;

	T* p = (T*) &m_list.Front();
	m_list.PopFront();

	// Call default constructor for new object.
	new (p) T;

	return p;
}

template <class T, typename AllocatorType,  class ThreadingModel>
inline T* DynamicPool<T, AllocatorType, ThreadingModel>::AllocateNoConstructor()
{
	typename ThreadingModel::Lock lock(m_threadData);

	if (m_list.Empty())
		Grow();

	if (m_list.Empty())
		return 0;

	T* p = (T*) &m_list.Front();
	m_list.PopFront();

	return p;
}

template <class T, typename AllocatorType,  class ThreadingModel>
inline T* DynamicPool<T, AllocatorType, ThreadingModel>::AllocatePrefetch()
{
	typename ThreadingModel::Lock lock(m_threadData);

	if (m_list.Empty())
		Grow();

	if (m_list.Empty())
		return 0;

	T* p = (T*) &m_list.Front();
	ListItem* pListItem = (ListItem*)p;
	DCBT(0, pListItem->LE_Next());
	m_list.PopFront();

	// Call default constructor for new object.
	new (p) T;

	return p;
}

template <class T, typename AllocatorType,  class ThreadingModel>
inline void DynamicPool<T, AllocatorType, ThreadingModel>::Prefetch()
{
	if (!m_list.Empty())
	{
		T* p = (T*) &m_list.Front();
		DCBT(0, p);
	}
}

template <class T, typename AllocatorType, class ThreadingModel>
inline void DynamicPool<T, AllocatorType, ThreadingModel>::Free(T* p)
{
	if (p == 0) return;

	typename ThreadingModel::Lock lock(m_threadData);

#ifndef CFG_GOLD
#if defined(PLATFORM_MACOS)
    U32 blockSize=PoolTypes<T>::N*sizeof(typename PoolTypes<T>::Slot);
#else
	U32 blockSize = PoolTypes<T>::N * sizeof(T);
#endif
	bool found = false;
	for(S32 i = 0; i < m_pool.Size(); ++i)
	{
		INTPTR pfree = (INTPTR)p;	// [Blur2PC][x64] addresses, not U32
#if defined(PLATFORM_MACOS)
        INTPTR memStart=(INTPTR)&(*PoolTypes<T>::BlockAt(m_pool[i]))[0];
#else
		INTPTR memStart = (INTPTR)m_pool[i];
#endif
		if(pfree >= memStart && pfree < memStart + blockSize)
		{
			found = true;
			break;
		}
	}
	BIZ_ASSERT_SPEED(found);
#endif

	// Call object's destructor.
	p->T::~T();

#if defined(PLATFORM_MACOS)
    m_list.PushFront(*new(p) ListItem);
#else
	m_list.PushFront((ListItem&) *p);
#endif
}



template <class T, typename AllocatorType, class ThreadingModel>
void DynamicPool<T, AllocatorType, ThreadingModel>::Grow()
{
#if defined(PLATFORM_MACOS)
    U8* data=AllocatorType::Allocate(PoolTypes<T>::AllocationBytes);
#else
	U8* data = AllocatorType::Allocate(sizeof(typename PoolTypes<T>::Block));
#endif

	if (data)
	{
#if defined(PLATFORM_MACOS)
        typename PoolTypes<T>::Block* newVector=PoolTypes<T>::BlockAt(data);
#else
		typename PoolTypes<T>::Block* newVector = (typename PoolTypes<T>::Block*) data;
#endif
		new (newVector) typename PoolTypes<T>::Block;

		m_pool.PushBack(data);

		for (int i = PoolTypes<T>::N - 1; i >= 0; i--)
		{
#if defined(PLATFORM_MACOS)
            m_list.PushFront(*new((*newVector)[i].bytes) ListItem);
#else
			m_list.PushFront((ListItem&) (*newVector)[i]);
#endif
		}

#if defined(PLATFORM_MACOS)
        m_sizeInBytes+=PoolTypes<T>::AllocationBytes;
#else
		m_sizeInBytes += sizeof(typename PoolTypes<T>::Block);
#endif
	}
}

}
#endif
