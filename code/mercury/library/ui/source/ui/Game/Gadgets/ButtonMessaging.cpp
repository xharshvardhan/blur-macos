#include "stdafx.h"

#include <misc_shared/Messaging/Messaging.h>

#include "ui/Game/Gadgets/ButtonController.h"
#include "ui/Game/Gadgets/ButtonMessaging.h"

namespace UI
{

// Store
void C_DeferredUiMessageStore::SetMessage(const Messaging::C_MessageBase* pMessage)
{
	if (pMessage != GetMessage())
	{
		SAFE_DELETE_ARY(m_pMessageData);

		if (pMessage != NULL) // TODO : pool this too?
		{
			m_pMessageData = new U8[pMessage->Size()];
			memcpy(m_pMessageData, pMessage, pMessage->Size());
		}
	}
}

// Queue
C_DeferredUiMessageQueue::C_DeferredUiMessageQueue()
{
}

C_DeferredUiMessageQueue::~C_DeferredUiMessageQueue()
{
#if defined(PLATFORM_MACOS)
	NativeCancelPendingMessages(); // [Blur2PC] Also retire an unprocessed/static queue before its real pool.
#endif
}

#if defined(PLATFORM_MACOS)
U32 C_DeferredUiMessageQueue::NativeCancelPendingMessages()
{
	U32 count=0;
	while(!m_MessageFIFO.Empty())
	{
		C_DeferredUiMessageStore* store=&m_MessageFIFO.Front();
		m_MessageFIFO.PopFront();
		m_MessagePool.Free(store); // [Blur2PC] Invokes the store destructor, releasing its copied byte payload.
		++count;
	}
	return count;
}
#endif

const U32 C_DeferredUiMessageQueue::ProcessMessages( C_ButtonController* pController )
{
	PERF_EVENT("UI.UiProcessMessages");
	assert(pController != NULL);

	U32 noMessages;
	for( noMessages = 0; !m_MessageFIFO.Empty(); ++noMessages )
	{
		C_DeferredUiMessageStore* pStore = &m_MessageFIFO.Front();
		m_MessageFIFO.PopFront();
#if defined(PLATFORM_MACOS)
		// [Blur2PC] A throwing callback must not strand the popped entry outside the FIFO.
		struct Retire {C_DeferredUiMessagePool& pool;C_DeferredUiMessageStore* store;~Retire(){pool.Free(store);}} retire{m_MessagePool,pStore};
#endif

		if( pStore->IsBroadcast() )
			pController->BroadcastMessageNow( pStore->GetBroadcastMask(), pStore->GetMessage() );
		else
			pController->SendMessageNow( pStore->GetDestButton(), pStore->GetMessage() );

#if !defined(PLATFORM_MACOS)
		m_MessagePool.Free( pStore );
#endif
	}

	return noMessages;
}

}; // namespace UI
