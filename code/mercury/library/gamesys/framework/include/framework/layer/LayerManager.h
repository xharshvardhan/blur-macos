#ifndef FRAMEWORK_LAYER_MANAGER_H
#define FRAMEWORK_LAYER_MANAGER_H

#ifdef _MSC_VER
#pragma once
#endif

#include "Framework/Application/ApplicationTypes.h"
#include "core/threads/Mutex.h"
#if defined(PLATFORM_MACOS) // [Blur2PC] Transfer creator failures to the application thread.
#include <exception>
#endif

namespace Framework
{

class C_Layer;
class C_LayerItem;
class C_LayerManager;

typedef Ut::UtSet<C_LayerItem> C_LayerList;

// C_LayerManagerThread
class C_LayerManagerThread : public MT::Thread
{
public:
	C_LayerManagerThread(C_LayerManager* pLayerManager);

	~C_LayerManagerThread();

	void        ThreadMain();

	void		SendQuitMessage();

	bool		ShouldQuit();

private:
	C_LayerManager* m_pLayerManager;
	MT::Event		m_EventSignalQuit;	
};


//! C_LayerItem
class C_LayerItem
{
public:
	C_LayerItem();
	C_LayerItem(C_Layer* pLayer);
	~C_LayerItem();

    C_Layer* GetLayerPtr() const;

	bool operator < (const C_LayerItem& other) const;

private:
	C_Layer* m_pLayer;
};

//! C_LayerManager

class C_LayerManager
{
friend class C_LayerManagerThread;
public:
	C_LayerManager();
	~C_LayerManager();

	void ShutDown();
#if defined(PLATFORM_MACOS) // [Blur2PC] A transactional application startup may retry after its worker was joined.
    void NativeRestart();
    // [Blur2PC] Finish in-flight creation before application-owned teardown.
    void NativeQuiesceCreation();
    // [Blur2PC] Call only after creator quiescence; retain application dependencies until each callback completes.
    void NativeDrainPendingDestruction();
    void NativeDestroyLayerNow(C_Layer* layer);
    // [Blur2PC] Worker resource/actor failures use the same checked main-thread handoff, without changing layer ownership.
    void NativeReportCreationFailure(const char* message);
    // A main-thread resize must not block while it already owns Display: an
    // async creator may hold this transaction while waiting for that device.
    // Ready objects must first pass the ordinary main-thread Running handoff.
    class NativeResizeLease {
    public:
        enum class Result { Busy, ReadyPending, CreationFailed, Acquired };
        explicit NativeResizeLease(C_LayerManager& manager);
        ~NativeResizeLease();
        NativeResizeLease(const NativeResizeLease&)=delete;
        NativeResizeLease& operator=(const NativeResizeLease&)=delete;
        explicit operator bool() const { return m_Manager!=nullptr; }
        Result GetResult() const { return m_Result; }
    private:
        C_LayerManager* m_Manager=nullptr;
        Result m_Result=Result::Busy;
    };
#endif

	//! Inserts a layer into the application.
	void InsertLayer(C_Layer* pLayer);

	//! Removes a layer from the application.
	void DestroyLayer(C_Layer* pLayer);

	//! Inserts a layer into the application.
	void InsertLayerAsync(C_Layer* pLayer);

	//! Removes a layer from the application.
	void DestroyLayerAsync(C_Layer* pLayer);

	// Creates and destroys layers.
	void Update();

	// Creates and destroys layers.
	bool UpdateAsync();

	HRESULT SendMessageToLayers(const U32 message);

	HRESULT SendMessageToLayer(C_Layer* pLayer, const U32 message);

	void Pause( const S32 pauseId );

	void Resume( const S32 pauseId );

	bool IsPaused( const S32 pauseId ) const;

	bool IsPaused() const;

private:

#if defined(PLATFORM_MACOS) // [Blur2PC] Failed Create callbacks own their resource rollback.
    bool NativeCreateLayer(C_Layer* layer);
    bool NativeHasCreationFailure();
    void NativeThrowCreationFailure();
#endif

	void CreatePendingLayers();

	void DestroyPendingLayers();

	void StartReadyLayers();

	bool CreatePendingAsyncLayers();

	bool DestroyPendingAsyncLayers();

	void DestroyAllLayers();

	void EraseLayer( C_Layer* pLayer );
	
	void SetStatusDirty();

	void WaitForStatusDirty();

private:
	Ut::UtSet<S32> m_PauseIdList;
	C_LayerList m_Layers;
	Ut::UtList<C_Layer*> m_LayersPendingCreate;
	Ut::UtList<C_Layer*> m_LayersPendingAsyncCreate;

	Ut::UtList<C_Layer*> m_ReadyLayers;

	Ut::UtList<C_Layer*> m_LayersPendingDestroy;
	Ut::UtList<C_Layer*> m_LayersPendingAsyncDestroy;

	MT::Event m_StatusDirtyEvent;
	MT::Lock m_AsyncCreateListLock;
	MT::Lock m_AsyncDestroyListLock;
	MT::Lock m_ReadyListLock;

#if defined(PLATFORM_MACOS) // [Blur2PC] Native workers must join before their owner and event members die.
    MT::Lock m_NativeLifecycleLock; // Create/Destroy ownership, including Ready publication.
    bool m_NativeShutdown = false;
    bool m_NativeCreationQuiesced = false;
    Ut::UtList<C_Layer*> m_NativeFailedLayers;
    std::exception_ptr m_NativeCreationError; // Protected, like failed/ready publication, by m_ReadyListLock.
#endif
	C_LayerManagerThread m_LayerManagerThread;
};

}; // namespace Framework

#endif // FRAMEWORK_LAYER_MANAGER_H
