#include "stdafx.h"
#include "racingphysics/racingphysics.h"
#if defined(PLATFORM_MACOS)
#include <memory>
#include <vector>
#include <stdexcept>
#include <limits>
#endif

namespace RacingPhysics
{

XtDb*				gXtDb;
XtPack*				gXtPack;
XtCallbackContext	gXtContext('RPHS');
#if defined(PLATFORM_MACOS)
// [Blur2PC] Track successful schema acquisition separately from borrowed DB storage.
bool gNativeXtAcquired=false;
bool gNativeOwnsXtDb=false;
#endif

void xt_open_callback(XtPackOpenStatus *status,void *context)
{
	LOGWARN( g_RacingPhysicsLog,"Xt Open Status: %s",status->message);
}

void CreateXt(XtDb* database)
{
#if defined(PLATFORM_MACOS)
	if(gXtPack)
	{
		if(database && database!=gXtDb)throw std::logic_error("RacingPhysics Xt pack already belongs to another database");
		return;
	}
	if(!XtInitialise())throw std::runtime_error("RacingPhysics Xt schema initialization failed");
	gNativeXtAcquired=true;gNativeOwnsXtDb=!database;
	try
	{
		std::unique_ptr<Sys::I_File> file(Sys::GetDefaultFileSystem()->Open("gamedata\\Xt\\racingphysics.bin",Sys::E_FileAccess_Read));
		if(!file)throw std::runtime_error("Required RacingPhysics Xt pack could not open");
		const auto length=file->Length();
		if(length<=0 || static_cast<U64>(length)>std::numeric_limits<U32>::max())throw std::runtime_error("Invalid RacingPhysics Xt pack size");
		std::vector<U8> data(static_cast<size_t>(length));
		if(!file->Read(data.data(),static_cast<U32>(length)))throw std::runtime_error("Required RacingPhysics Xt pack read failed");
		gXtDb=database?database:XtDbCreate(XtGetApplicationSchema(),5000,&gXtContext);
		if(!gXtDb)throw std::runtime_error("RacingPhysics Xt database creation failed");
		XtPackOpenParams options;options.callback=xt_open_callback;
		gXtPack=XtPackOpen(gXtDb,data.data(),data.size(),&options);
		if(!gXtPack)throw std::runtime_error("Required RacingPhysics Xt pack conversion failed");
		if(gNativeOwnsXtDb)XtNetworkStart(gXtDb);
	}
	catch(...){DestroyXt();throw;}
#else
	XtInitialise();
	
	Sys::I_File* pFile = Sys::GetDefaultFileSystem()->Open("gamedata\\Xt\\racingphysics.bin", Sys::E_FileAccess_Read);

	if (pFile)
	{
		int length = (int) pFile->Length();
		U8* data = new U8[length];

		if (data)
		{
			if (pFile->Read(data, length))
			{
				XtPackOpenParams params;
				params.callback=xt_open_callback;

				if (!database)
					gXtDb = XtDbCreate(XtGetApplicationSchema(), 5000, &gXtContext);
				else
					gXtDb = database;

				RP_ASSERT(gXtDb);
				if (gXtDb)
					gXtPack	= XtPackOpen(gXtDb, data, length, &params);

				if (!database)
					XtNetworkStart(gXtDb);
			}
			delete [] data;
		}

		delete pFile;
	}
#endif
}

void DestroyXt()
{
#if defined(PLATFORM_MACOS)
	// [Blur2PC] Partial failure and repeated final retirement are safe; DB remains borrowed.
	if(gXtPack){XtPackClose(gXtDb,gXtPack);gXtPack=NULL;}
	if(gNativeOwnsXtDb && gXtDb)XtDbDestroy(gXtDb);
	gXtDb=NULL;gNativeOwnsXtDb=false;
	if(gNativeXtAcquired){gNativeXtAcquired=false;XtShutdown();}
#else
	XtShutdown();
#endif
}

void UpdateXt()
{
	XtNetworkUpdate();
}

}
