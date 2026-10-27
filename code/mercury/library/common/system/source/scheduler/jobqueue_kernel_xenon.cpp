#include "SystemPch.h"
#include <core/Core.h>
#include "system/scheduler/jobqueue.h"
//#include "vmath.h"

namespace Scheduler
{

extern TaskPool jqPool;

#if !defined(PLATFORM_MACOS)
extern HANDLE jqNewJobAdded;
#endif

#if defined(PLATFORM_MACOS)
extern std::atomic<bool> jqStopSignal;
#else
extern bool jqStopSignal;
#endif

extern int jqSleepingWorkers;
extern int jqWorkerAwakeCount;
#if defined(PLATFORM_MACOS)
// [Blur2PC] Event handles and thread-local diagnostics for the native worker path.
extern MT::Event* g_WorkerAlert[8];
std::atomic<const char*> g_pszJobName[10];
thread_local int g_iWorkerNumber = 0;
#else
extern HANDLE g_WorkerAlert[8];
char *g_pszJobName[10];
__declspec(thread) int g_iWorkerNumber = 6;
#endif

void jqExecuteBatch(Task* Batch)
{
	g_pszJobName[g_iWorkerNumber] = (char*)Batch->Module->Name;

//	tlBeginNamedEvent(0, Batch->Module->Name);
	Batch->Module->Code( Batch );
//	tlEndNamedEvent();

	g_pszJobName[g_iWorkerNumber] = NULL;
}

// The xenon reserves cores 2-5 for OS operations for 1ms (1.2 for pal) before every vsync.
// This can cause major lags if the jobqueue is currently executing a batch, as it gets fully
// taken over by this system reservation.

// this function is basically obsolete
FORCE_INLINE bool jqSafeToPopBatch(jqWorker* Worker, jqPriority Priority, bool IsLocal)
{
	return true;
}

bool jqCanBatchExecute( Task* Batch, jqWorker* Worker )
{
	return true;
}

bool jqWorkerSleep(jqWorker *Worker, int iWorkerID)
{
#if defined(PLATFORM_MACOS)
    g_WorkerAlert[iWorkerID]->Wait();
#else
	WaitForSingleObject( g_WorkerAlert[iWorkerID], INFINITE );
#endif

	return jqStopSignal == 0;
}

void jqWorkerLoop(jqWorker* Worker, int GroupID, int iWorkerID);

int GetWorkerID()
{
	return g_iWorkerNumber;
}

// Thread which does the actual work.
#if defined(PLATFORM_MACOS)
U32 jqWorkerThread(void* _this)
#else
DWORD WINAPI jqWorkerThread( void* _this )
#endif
{
	g_iWorkerNumber = ((jqWorker*)_this)->Processor;

	jqWorkerLoop((jqWorker*)_this, JQ_GROUP_NONE, g_iWorkerNumber);
	return 0;
}

} // namespace Scheduler
