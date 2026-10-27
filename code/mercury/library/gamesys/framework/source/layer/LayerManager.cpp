#include "FrameworkPch.h"
#include "Framework/Layer/Layer.h"
#include "Framework/Layer/LayerManager.h"
#if defined(PLATFORM_MACOS) // [Blur2PC] Report real creation failures on the application thread.
#include <stdexcept>
#include <sstream>
#endif

namespace Framework
{

// C_LayerManagerThread

C_LayerManagerThread::C_LayerManagerThread( C_LayerManager* pLayerManager )
#if defined(PLATFORM_MACOS) // [Blur2PC] A private stop token must not alias another worker's auto-reset named event.
    : m_EventSignalQuit()
#else
	: m_EventSignalQuit("EventSignalQuit")
#endif
	, m_pLayerManager(pLayerManager)
{

}

C_LayerManagerThread::~C_LayerManagerThread()
{

}

void C_LayerManagerThread::ThreadMain()
{
	
	//C_Manager* pManager = C_Manager::GetInstance();
	m_pLayerManager->SetStatusDirty();

	while(!ShouldQuit())
	{
		//WaitIfPaused();

		bool didSomething = m_pLayerManager->UpdateAsync();

		if(!didSomething)
		{
			m_pLayerManager->WaitForStatusDirty();
		}
	}
}

bool C_LayerManagerThread::ShouldQuit()
{
	return m_EventSignalQuit.Wait(0);
}

void C_LayerManagerThread::SendQuitMessage()
{
	m_EventSignalQuit.Signal();
}


// C_LayerItem

C_LayerItem::C_LayerItem()
	: m_pLayer( NULL )
{
}

C_LayerItem::C_LayerItem( C_Layer* pLayer )
	: m_pLayer( pLayer )
{
}

C_LayerItem::~C_LayerItem()
{
}

bool C_LayerItem::operator < (const C_LayerItem& other) const
{
	// if priorities are equal, order by pointer
	if( m_pLayer->GetLayerPriority() == other.m_pLayer->GetLayerPriority() )
	{
		return (m_pLayer < other.m_pLayer);
	}

	return m_pLayer->GetLayerPriority() < other.m_pLayer->GetLayerPriority();
}

C_Layer* C_LayerItem::GetLayerPtr() const
{
	return m_pLayer;
}

// C_LayerManager

C_LayerManager::C_LayerManager()
	: m_LayerManagerThread(this)
	, m_StatusDirtyEvent("LayerStatusDirtyEvent")
{
	m_LayerManagerThread.Start(0,0,"LayerManagerThread");
	SetStatusDirty();
}

C_LayerManager::~C_LayerManager()
{
#if defined(PLATFORM_MACOS) // [Blur2PC] Close the native worker while derived state is still alive.
    ShutDown();
#endif
}

#if defined(PLATFORM_MACOS) // [Blur2PC] Restart the actual joined worker, not just the application's initialized flag.
void C_LayerManager::NativeRestart()
{
    if(!m_NativeShutdown)return;
    {
        MT::AutoLock lock(m_ReadyListLock);
        assert(m_NativeFailedLayers.Empty());
        m_NativeCreationError=nullptr;
    }
    while(m_LayerManagerThread.ShouldQuit()){} // Consume any residual auto-reset quit signal after the joined thread.
    m_LayerManagerThread.Start(0,0,"LayerManagerThread");m_NativeShutdown=false;m_NativeCreationQuiesced=false;SetStatusDirty();
}
#endif

#if defined(PLATFORM_MACOS) // [Blur2PC] Join creators before their application-owned dependencies are released.
void C_LayerManager::NativeQuiesceCreation()
{
    if(m_NativeCreationQuiesced)return;
    m_LayerManagerThread.SendQuitMessage();
    SetStatusDirty();
    m_LayerManagerThread.Join();
    m_NativeCreationQuiesced=true;
    StartReadyLayers();
}
#endif

#if defined(PLATFORM_MACOS)
// [Blur2PC] Application teardown needs dependency order, not the active set's pointer order.
void C_LayerManager::NativeDrainPendingDestruction()
{
    assert(m_NativeCreationQuiesced);
    do {
        DestroyPendingLayers();
        DestroyPendingAsyncLayers();
    } while(!m_LayersPendingDestroy.Empty() || !m_LayersPendingAsyncDestroy.Empty());
}

void C_LayerManager::NativeDestroyLayerNow(C_Layer* layer)
{
    assert(m_NativeCreationQuiesced);
    assert(layer);
    const E_LayerState state=layer->GetLayerState();
    if(state==E_LayerState_Ready) {
        // Create succeeded, but a different creator failed before this layer could run.
        bool found=false;
        {
            MT::AutoLock lock(m_ReadyListLock);
            for(auto it=m_ReadyLayers.Begin();it!=m_ReadyLayers.End();++it)
                if(*it==layer){m_ReadyLayers.Erase(it);found=true;break;}
        }
        assert(found);
        SendMessageToLayer(layer,E_LayerMessage_Destroy);
        layer->SetLayerState(E_LayerState_Constructed);
        delete layer;
        return;
    }
    if(state==E_LayerState_Constructed || state==E_LayerState_PendingCreate) {
        // Failed callbacks roll back locally; never-created objects need no Destroy callback either.
        DestroyLayer(layer);
        return;
    }
    if(state==E_LayerState_Running) DestroyLayer(layer);
    assert(layer->GetLayerState()==E_LayerState_PendingDestroy ||
           layer->GetLayerState()==E_LayerState_PendingAsyncDestroy);

    bool active=true;
    bool found=false;
    for(auto it=m_LayersPendingDestroy.Begin();it!=m_LayersPendingDestroy.End();++it) {
        if(*it==layer) {m_LayersPendingDestroy.Erase(it);found=true;break;}
    }
    if(!found) {
        // Update may already have transferred asynchronous destruction to the joined worker's queue.
        for(auto it=m_LayersPendingAsyncDestroy.Begin();it!=m_LayersPendingAsyncDestroy.End();++it) {
            if(*it==layer) {m_LayersPendingAsyncDestroy.Erase(it);found=true;active=false;break;}
        }
    }
    assert(found);
    if(active) EraseLayer(layer);
    SendMessageToLayer(layer,E_LayerMessage_Destroy);
    layer->SetLayerState(E_LayerState_Constructed);
    delete layer;
}
#endif

void C_LayerManager::ShutDown()
{
#if defined(PLATFORM_MACOS) // [Blur2PC] Quiesce callbacks before draining queues and destroying layers.
    if (m_NativeShutdown) return;
    m_NativeShutdown = true;
    NativeQuiesceCreation();
    while (!m_LayersPendingAsyncCreate.Empty()) {
        C_Layer* layer = *m_LayersPendingAsyncCreate.Begin();
        m_LayersPendingAsyncCreate.Erase(m_LayersPendingAsyncCreate.Begin());
        delete layer;
    }
    StartReadyLayers();
#endif
	if( m_Layers.Size() != 0 )
	{
		LOGWARN( g_Log, "Layers still active on exit" );
	}

	Ut::UtList<C_Layer*>::Iterator it = m_LayersPendingCreate.Begin();
	while(it != m_LayersPendingCreate.End())
	{
		C_Layer* pLayer = *it;

		m_LayersPendingCreate.Erase(it);
		SAFE_DELETE_PTR(pLayer);

		it = m_LayersPendingCreate.Begin();
	}

	// TODO: shutdown async layers
	SetStatusDirty();


	DestroyAllLayers();
	DestroyPendingLayers();
#if defined(PLATFORM_MACOS) // [Blur2PC] The joined worker can no longer service deferred destruction.
    DestroyPendingAsyncLayers();
    // Application-owned layers were retired in dependency order above; these are unclaimed remnants.
    while(!m_ReadyLayers.Empty()) NativeDestroyLayerNow(*m_ReadyLayers.Begin());
    while(!m_NativeFailedLayers.Empty()) NativeDestroyLayerNow(*m_NativeFailedLayers.Begin());
#endif
}

void C_LayerManager::SetStatusDirty()
{
	m_StatusDirtyEvent.Signal();
}

void C_LayerManager::WaitForStatusDirty()
{
	m_StatusDirtyEvent.Wait();
}

void C_LayerManager::InsertLayer(C_Layer* pLayer)
{
	assert(pLayer);

	assert(pLayer->GetLayerState() == E_LayerState_Constructed);

	m_LayersPendingCreate.PushBack(pLayer);
	
	pLayer->SetLayerState( E_LayerState_PendingCreate );
}

void C_LayerManager::DestroyLayer(C_Layer* pLayer)
{
	assert(pLayer);
	#if defined(PLATFORM_MACOS) // [Blur2PC] Failed native startup can own unstarted layers.
    if(pLayer->GetLayerState()==E_LayerState_Constructed){
        {
            MT::AutoLock lock(m_ReadyListLock);
            for(auto it=m_NativeFailedLayers.Begin();it!=m_NativeFailedLayers.End();++it)
                if(*it==pLayer){m_NativeFailedLayers.Erase(it);break;}
        }
        delete pLayer;return;
    }
    if(pLayer->GetLayerState()==E_LayerState_PendingCreate){
        // Cancel only work still owned by the caller's synchronous queue.
        // Async/in-flight creation remains worker-owned until explicitly quiesced.
        for(auto it=m_LayersPendingCreate.Begin();it!=m_LayersPendingCreate.End();++it){
            if(*it==pLayer){m_LayersPendingCreate.Erase(it);delete pLayer;return;}
        }
        if(m_NativeCreationQuiesced){
            for(auto it=m_LayersPendingAsyncCreate.Begin();it!=m_LayersPendingAsyncCreate.End();++it){
                if(*it==pLayer){m_LayersPendingAsyncCreate.Erase(it);delete pLayer;return;}
            }
        }
    }
	#endif

	assert(pLayer->GetLayerState() == E_LayerState_Running);

	// remove from active lists
	//EraseLayer( pLayer );
	m_LayersPendingDestroy.PushBack(pLayer);

	pLayer->SetLayerState( E_LayerState_PendingDestroy );
}

void C_LayerManager::InsertLayerAsync(C_Layer* pLayer)
{
	MT::AutoLock autoLock(m_AsyncCreateListLock);

	assert(pLayer);

	assert(pLayer->GetLayerState() == E_LayerState_Constructed);

	m_LayersPendingAsyncCreate.PushBack(pLayer);

	pLayer->SetLayerState( E_LayerState_PendingCreate );

	SetStatusDirty();
}

void C_LayerManager::DestroyLayerAsync(C_Layer* pLayer)
{
	assert(pLayer);

	assert(pLayer->GetLayerState() == E_LayerState_Running);

	// remove from active list
	//EraseLayer( pLayer );

	// just state is differnt, both async and non are put into this list
	m_LayersPendingDestroy.PushBack(pLayer);

	pLayer->SetLayerState( E_LayerState_PendingAsyncDestroy );

	SetStatusDirty();
}

void C_LayerManager::DestroyAllLayers()
{
	C_LayerList::Iterator it = m_Layers.Begin();
	while(it != m_Layers.End())
	{
		C_LayerItem layerItem = *it;
		C_Layer* pLayer = layerItem.GetLayerPtr();
		it++;

		if(pLayer->GetLayerState() == E_LayerState_Running)
		{
			DestroyLayer(pLayer);
		}
	}
}

void C_LayerManager::Update()
{
#if defined(PLATFORM_MACOS)
	NativeThrowCreationFailure();
#endif
	DestroyPendingLayers();
	CreatePendingLayers();
#if defined(PLATFORM_MACOS)
	NativeThrowCreationFailure();
#endif

	//UpdateAsync();

	StartReadyLayers();
#if defined(PLATFORM_MACOS)
	NativeThrowCreationFailure();
#endif
}

bool C_LayerManager::UpdateAsync()
{
	bool didSomething = false;
	didSomething |= DestroyPendingAsyncLayers();
	didSomething |= CreatePendingAsyncLayers();
	return didSomething;
}

void C_LayerManager::DestroyPendingLayers()
{
	//TODO: Changed this to use a reverse iterator as there were layers depending on the existence of layers
	//created previously (i.e. the game layer releasing menu items from the debug layer). Destroying them in reverse 
	//order is more likely to work, but there still could be dependancies between layers. 

	Ut::UtList<C_Layer*>::ReverseIterator it = m_LayersPendingDestroy.ReverseBegin();
	while(it != m_LayersPendingDestroy.ReverseEnd())
	{
		C_Layer* pLayer = *it;

		EraseLayer(pLayer);
		m_LayersPendingDestroy.Erase(it);

		if(pLayer->m_LayerState == E_LayerState_PendingAsyncDestroy)
		{
			MT::AutoLock autoLock(m_AsyncDestroyListLock);
			m_LayersPendingAsyncDestroy.PushBack(pLayer);
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Publish work before notifying: DestroyLayerAsync's earlier wake can be consumed before this transfer.
			SetStatusDirty();
#endif
		}
		else
		{
			SendMessageToLayer( pLayer, E_LayerMessage_Destroy );

			pLayer->SetLayerState( E_LayerState_Constructed );
			SAFE_DELETE_PTR( pLayer );
		}

		it = m_LayersPendingDestroy.ReverseBegin();
	}
}

bool C_LayerManager::DestroyPendingAsyncLayers()
{
	bool didSomething = false;

	Ut::UtList<C_Layer*> layersPendingDestroy;
	{
		MT::AutoLock autoLock(m_AsyncDestroyListLock);

		Ut::UtList<C_Layer*>::ReverseIterator it = m_LayersPendingAsyncDestroy.ReverseBegin();
		while(it != m_LayersPendingAsyncDestroy.ReverseEnd())
		{
			C_Layer* pLayer = *it;

			m_LayersPendingAsyncDestroy.Erase(it);

			layersPendingDestroy.PushBack(pLayer);

			it = m_LayersPendingAsyncDestroy.ReverseBegin();
		}
	}

	//TODO: Changed this to use a reverse iterator as there were layers depending on the existence of layers
	//created previously (i.e. the game layer releasing menu items from the debug layer). Destroying them in reverse 
	//order is more likely to work, but there still could be dependancies between layers. 

	Ut::UtList<C_Layer*>::ReverseIterator it = layersPendingDestroy.ReverseBegin();
	while(it != layersPendingDestroy.ReverseEnd())
	{
		C_Layer* pLayer = *it;

		SendMessageToLayer( pLayer, E_LayerMessage_Destroy );

		layersPendingDestroy.Erase(it);
		pLayer->SetLayerState( E_LayerState_Constructed );
		SAFE_DELETE_PTR( pLayer );

		didSomething = true;

		it = layersPendingDestroy.ReverseBegin();
	}

	return didSomething;
}


void C_LayerManager::EraseLayer( C_Layer* pLayer )
{
	C_LayerList::Iterator it = m_Layers.Begin();
	while(it != m_Layers.End())
	{
		const C_LayerItem layerItem = *it;
		if( layerItem.GetLayerPtr() == pLayer )
		{
			m_Layers.Erase( layerItem );
			return;
		}

		it++;
	}

	assert( 0 && "EraseLayer: layer not found" );
}

void C_LayerManager::CreatePendingLayers()
{
#if defined(PLATFORM_MACOS)
	if(NativeHasCreationFailure())return;
#endif
	Ut::UtList<C_Layer*>::Iterator it = m_LayersPendingCreate.Begin();
	while(it != m_LayersPendingCreate.End())
	{
#if defined(PLATFORM_MACOS)
		if(NativeHasCreationFailure())return;
#endif
		C_Layer* pLayer = *it;

		m_LayersPendingCreate.Erase(it);

#if defined(PLATFORM_MACOS) // [Blur2PC] Keep a failed object owned, but never publish it as Ready.
		if(!NativeCreateLayer(pLayer))return;
#else
		HRESULT hr = SendMessageToLayer( pLayer, E_LayerMessage_Create );
		if( FAILED(hr) )
		{
			LOGWARN( g_Log, "Failed to create layer" );
		}

		{
			MT::AutoLock autoLock(m_ReadyListLock);

			m_ReadyLayers.PushBack( pLayer );
			pLayer->SetLayerState( E_LayerState_Ready );
		}
#endif

		it = m_LayersPendingCreate.Begin();
	}
}

bool C_LayerManager::CreatePendingAsyncLayers()
{
	bool didSomething = false;
#if defined(PLATFORM_MACOS)
	if(NativeHasCreationFailure())return false;
#endif

	Ut::UtList<C_Layer*> layersPendingCreate;

	{
		MT::AutoLock autoLock(m_AsyncCreateListLock);
	
		// copy list of layers pending create
		Ut::UtList<C_Layer*>::Iterator it = m_LayersPendingAsyncCreate.Begin();
		while(it != m_LayersPendingAsyncCreate.End())
		{
			C_Layer* pLayer = *it;
			m_LayersPendingAsyncCreate.Erase(it);

			layersPendingCreate.PushBack(pLayer);

			it = m_LayersPendingAsyncCreate.Begin();
		}
	}

	Ut::UtList<C_Layer*>::Iterator it = layersPendingCreate.Begin();
	while(it != layersPendingCreate.End())
	{
#if defined(PLATFORM_MACOS) // [Blur2PC] Return unstarted work to its owned cancellation queue.
		if(NativeHasCreationFailure()){
			MT::AutoLock lock(m_AsyncCreateListLock);
			while(!layersPendingCreate.Empty()){
				C_Layer* pending=*layersPendingCreate.Begin();
				layersPendingCreate.Erase(layersPendingCreate.Begin());
				m_LayersPendingAsyncCreate.PushBack(pending);
			}
			break;
		}
#endif
		C_Layer* pLayer = *it;

		layersPendingCreate.Erase(it);

#if defined(PLATFORM_MACOS) // [Blur2PC] Catch and hand off all Create exceptions instead of escaping the worker.
		NativeCreateLayer(pLayer);
#else
		HRESULT hr = SendMessageToLayer( pLayer, E_LayerMessage_Create );
		if( FAILED(hr) )
		{
			LOGWARN( g_Log, "Failed to create layer" );
		}

		{
			MT::AutoLock autoLock(m_ReadyListLock);

			m_ReadyLayers.PushBack( pLayer );
			pLayer->SetLayerState( E_LayerState_Ready );
		}
#endif


		didSomething = true;

		it = layersPendingCreate.Begin();
	}

	return didSomething;
}

void C_LayerManager::StartReadyLayers()
{
	MT::AutoLock autoLock(m_ReadyListLock);
#if defined(PLATFORM_MACOS)
	if(m_NativeCreationError)return;
#endif

	Ut::UtList<C_Layer*>::Iterator it = m_ReadyLayers.Begin();
	while(it != m_ReadyLayers.End())
	{
		C_Layer* pLayer = *it;

		m_ReadyLayers.Erase(it);

		m_Layers.Insert( C_LayerItem(pLayer) );
		pLayer->SetLayerState( E_LayerState_Running );

		it = m_ReadyLayers.Begin();
	}
}

HRESULT C_LayerManager::SendMessageToLayers(const U32 message)
{
	HRESULT hr = S_OK;

	// layers list can only be modified in the main thread

	C_LayerList::Iterator it = m_Layers.Begin();
	while(it != m_Layers.End())
	{
		C_LayerItem layerItem = *it;
		C_Layer* pLayer = layerItem.GetLayerPtr();
		it++;

		SendMessageToLayer(pLayer, (E_LayerMessage)message);
	}

	return hr;
}

HRESULT C_LayerManager::SendMessageToLayer(C_Layer* pLayer, const U32 message)
{
	// filter out all but create and destroy messages if layer is not running or disabled
	if((pLayer->GetLayerState() != E_LayerState_Running)
	    || (!pLayer->m_Enabled))
	{
		if(	( message != E_LayerMessage_Create ) &&
			( message != E_LayerMessage_Destroy ) &&
			( message != E_LayerMessage_Resize ) )
			return S_OK;
	}

#if defined(PLATFORM_MACOS) // [Blur2PC] Preserve the real result; Create failure must reach its owner.
	return pLayer->ProcessMessage((E_LayerMessage)message);
#else
	pLayer->ProcessMessage((E_LayerMessage)message);

	return S_OK;
#endif
}

#if defined(PLATFORM_MACOS) // [Blur2PC] Publish creator ownership and the first error under the same lock.
// [Blur2PC] Actor/resource callbacks may run on creators or job workers; never unwind those threads.
void C_LayerManager::NativeReportCreationFailure(const char* message)
{
    const auto error=std::make_exception_ptr(std::runtime_error(message));
    MT::AutoLock lock(m_ReadyListLock);
    if(!m_NativeCreationError)m_NativeCreationError=error;
}

bool C_LayerManager::NativeHasCreationFailure()
{
    MT::AutoLock lock(m_ReadyListLock);
    return bool(m_NativeCreationError);
}

void C_LayerManager::NativeThrowCreationFailure()
{
    std::exception_ptr error;
    {MT::AutoLock lock(m_ReadyListLock);error=m_NativeCreationError;}
    if(error)std::rethrow_exception(error);
}

bool C_LayerManager::NativeCreateLayer(C_Layer* layer)
{
    HRESULT result=S_OK;
    std::exception_ptr thrown;
    try{result=SendMessageToLayer(layer,E_LayerMessage_Create);}
    catch(...){thrown=std::current_exception();}
    std::exception_ptr error;
    if(thrown || FAILED(result)){
        std::ostringstream message;
        message<<"Failed to create native layer '"<<layer->GetName()<<"'";
        if(thrown){
            try{std::rethrow_exception(thrown);}
            catch(const std::exception& exception){message<<": "<<exception.what();}
            catch(...){message<<": non-standard exception";}
        }else message<<" (HRESULT 0x"<<std::hex<<U32(result)<<")";
        LOGERROR(g_Log,"%s",message.str().c_str());
        error=std::make_exception_ptr(std::runtime_error(message.str()));
    }
    MT::AutoLock lock(m_ReadyListLock);
    if(error){
        // The failed callback must already have undone its resources. Retain the object
        // until application-owned dependency teardown; sending Destroy would invent success.
        layer->SetLayerState(E_LayerState_Constructed);
        m_NativeFailedLayers.PushBack(layer);
        if(!m_NativeCreationError)m_NativeCreationError=error;
        return false;
    }
    m_ReadyLayers.PushBack(layer);
    layer->SetLayerState(E_LayerState_Ready);
    return true;
}
#endif

void C_LayerManager::Pause( const S32 pauseId )
{
	m_PauseIdList.Insert( pauseId );
}

void C_LayerManager::Resume( const S32 pauseId )
{
	m_PauseIdList.Erase( pauseId );
}

bool C_LayerManager::IsPaused() const
{
	return (m_PauseIdList.Size() > 0);
}

bool C_LayerManager::IsPaused( const S32 pauseId ) const
{
	Ut::UtSet<S32>::ConstIterator it = m_PauseIdList.Begin();
	while( it != m_PauseIdList.End() )
	{
		S32 id = *it;
		if( id == pauseId )
			return true;
		++it;
	}

	return false;
}

}; // namespace Framework
