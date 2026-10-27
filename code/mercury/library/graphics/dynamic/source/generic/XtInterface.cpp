#include "stdafx.h"

#include <Dynamic/Generic/XtInterface.h>

#include <Misc_Shared/Curve/HermiteCurve.h>
#include <Dynamic/Common.h>
#include <Dynamic/Enums.h>
#if defined(PLATFORM_MACOS)
#include <memory>
#include <vector>
#include <stdexcept>
#include <limits>
#endif

XT_TYPE(DynamicEffect::XtInterface::Light, "Effect_Light")
	XT_FIELD(receive, "receive")
	XT_FIELD(emit, "emit")
	XT_FIELD(radius, "radius")
	XT_FIELD(intensity, "intensity")
	XT_FIELD(priority, "priority")
XT_END_TYPE

XT_TYPE(DynamicEffect::XtInterface::HermiteKeyFrame, "Effect_HermiteKeyFrame")
	XT_FIELD(value, "value")
	XT_FIELD(tangentIn, "inTangent")
	XT_FIELD(tangentOut, "outTangent")
	XT_FIELD(time, "time")
XT_END_TYPE

XT_TYPE(DynamicEffect::XtInterface::U32Dim2, "Effect_U32Dim2")
	XT_FIELD(width, "width")
	XT_FIELD(height, "height")
XT_END_TYPE

XT_TYPE(DynamicEffect::XtInterface::U32Dim3, "Effect_U32Dim3")
	XT_FIELD(width, "width")
	XT_FIELD(height, "height")
	XT_FIELD(depth, "depth")
XT_END_TYPE

XT_TYPE(DynamicEffect::XtInterface::ColourTime, "Effect_ColourTime")
	XT_FIELD(colour, "colour")
	XT_FIELD(time, "time")
XT_END_TYPE

XT_TYPE(DynamicEffect::E_Atlas, "Effect_Atlas")
	XT_OPTION(DynamicEffect::E_Atlas_Default1x1, "Default1x1")
	XT_OPTION(DynamicEffect::E_Atlas_Square2x2, "Square2x2")
	XT_OPTION(DynamicEffect::E_Atlas_Square4x4, "Square4x4")
	XT_OPTION(DynamicEffect::E_Atlas_Square8x8, "Square8x8")
	XT_OPTION(DynamicEffect::E_Atlas_Strip2x1, "Strip2x1")
	XT_OPTION(DynamicEffect::E_Atlas_Strip4x1, "Strip4x1")
	XT_OPTION(DynamicEffect::E_Atlas_Strip4x2, "Strip4x2")
	XT_OPTION(DynamicEffect::E_Atlas_Strip8x2, "Strip8x2")
	XT_OPTION(DynamicEffect::E_Atlas_Strip8x4, "Strip8x4")
XT_END_TYPE

XT_TYPE(DynamicEffect::E_AtlasType, "Effect_AtlasType")
	XT_OPTION(DynamicEffect::E_AtlasType_Random, "Random")
	XT_OPTION(DynamicEffect::E_AtlasType_Animated, "Animated")
XT_END_TYPE

XT_LIBRARY(GraphicsDynamic)
	XT_REGISTER_TYPE(DynamicEffect::XtInterface::Light)
	XT_REGISTER_TYPE(DynamicEffect::XtInterface::HermiteKeyFrame)
	XT_REGISTER_TYPE(DynamicEffect::XtInterface::U32Dim2)
	XT_REGISTER_TYPE(DynamicEffect::XtInterface::U32Dim3)
	XT_REGISTER_TYPE(DynamicEffect::XtInterface::ColourTime)
	XT_REGISTER_TYPE(DynamicEffect::E_Atlas)
	XT_REGISTER_TYPE(DynamicEffect::E_AtlasType)
XT_END_LIBRARY

namespace DynamicEffect
{
namespace XtInterface
{

C_Renderer*			g_pRenderer = NULL;

// Xt loading

XtDb*				g_pXtDb	= NULL;
XtPack*				g_pXtPack = NULL;
XtCallbackContext	g_XtContext('P');
#if defined(PLATFORM_MACOS)
// [Blur2PC] Borrowed DB ownership and the exact successful schema acquisition.
bool g_NativeXtAcquired=false;
bool g_NativeOwnsXtDb=false;
#endif

void XtOpenCallback(struct XtPackOpenStatus* status, void* context)
{
	DebugLn("%s", status->message);
}

void PackOpen(XtDb* pDatabase, const Util::C_String& basePath, Util::C_String packName)
{
#if defined(PLATFORM_MACOS)
	if(g_pXtPack)
	{
		if(pDatabase && pDatabase!=g_pXtDb)throw std::logic_error("DynamicEffect Xt pack already belongs to another database");
		return;
	}
	if(!XtInitialise())throw std::runtime_error("DynamicEffect Xt schema initialization failed");
	g_NativeXtAcquired=true;
	g_NativeOwnsXtDb=!pDatabase;
	try
	{
		if(packName.Length()==0)packName="dynamiceffects";
		packName+=".bin";
		std::unique_ptr<Sys::I_File> file(Sys::GetDefaultFileSystem()->Open(basePath+packName,Sys::E_FileAccess_Read));
		if(!file)throw std::runtime_error("Required DynamicEffect Xt pack could not open");
		const auto length=file->Length();
		if(length<=0 || static_cast<U64>(length)>std::numeric_limits<U32>::max())throw std::runtime_error("Invalid DynamicEffect Xt pack size");
		std::vector<U8> data(static_cast<size_t>(length));
		if(!file->Read(data.data(),static_cast<U32>(length)))throw std::runtime_error("Required DynamicEffect Xt pack read failed");
		g_pXtDb=pDatabase?pDatabase:XtDbCreate(XtGetApplicationSchema(),5000,&g_XtContext);
		if(!g_pXtDb)throw std::runtime_error("DynamicEffect Xt database creation failed");
		XtPackOpenParams options;options.callback=XtOpenCallback;
		g_pXtPack=XtPackOpen(g_pXtDb,data.data(),data.size(),&options);
		if(!g_pXtPack)throw std::runtime_error("Required DynamicEffect Xt pack conversion failed");
		if(g_NativeOwnsXtDb)XtNetworkStart(g_pXtDb);
	}
	catch(...){PackClose();throw;}
#else
	if(!g_pXtPack)
	{
		XtInitialise();
		
		if (packName.Length() == 0)
		{
			packName = "dynamiceffects";
		}

		packName += ".bin";

		Sys::I_File* pFile = Sys::GetDefaultFileSystem()->Open(basePath + packName, Sys::E_FileAccess_Read);

		if (pFile)
		{
			int length = (int) pFile->Length();
			U8* data = new U8[length];

			if (data)
			{
				if (pFile->Read(data, length))
				{
					XtPackOpenParams params;
					params.callback = XtOpenCallback;

					if (!pDatabase)
						g_pXtDb = XtDbCreate(XtGetApplicationSchema(), 5000, &g_XtContext);
					else
						g_pXtDb = pDatabase;

					assert(g_pXtDb);
					if (g_pXtDb)
						g_pXtPack = XtPackOpen(g_pXtDb, data, length, &params);

					if (!pDatabase)
						XtNetworkStart(g_pXtDb);
				}
				delete [] data;
			}

			delete pFile;
		}
	}
#endif
}

void PackClose()
{
#if defined(PLATFORM_MACOS)
	// [Blur2PC] Close records before their DB/schema, and never destroy a borrowed DB.
	if(g_pXtPack){XtPackClose(g_pXtDb,g_pXtPack);g_pXtPack=NULL;}
	if(g_NativeOwnsXtDb && g_pXtDb)XtDbDestroy(g_pXtDb);
	g_pXtDb=NULL;g_NativeOwnsXtDb=false;
	if(g_NativeXtAcquired){g_NativeXtAcquired=false;XtShutdown();}
#else
	if(g_pXtPack)
	{
		XtPackClose(g_pXtDb, g_pXtPack);
		g_pXtPack = NULL;
	}
#endif
}

XtPack* GetPack()
{
	return g_pXtPack;
}

void Initialise(C_Renderer* pRenderer)
{
	g_pRenderer = pRenderer;
}

C_Renderer*	Renderer()
{
	return g_pRenderer;
}

bool CreateCurve(const XtArray<XtInterface::HermiteKeyFrame>& keyframes, LookupTable& lookupTable, S32 nSteps, S32 iElement)
{
	Curve::C_HermiteCurve curve;
	bool bSuccess = true;
	// add keyframes to curve
	for(S32 i = 0; i < keyframes.count; ++i)
	{
		bSuccess = bSuccess && curve.AddKeyFrame(keyframes[i].time, keyframes[i].value, keyframes[i].tangentIn, keyframes[i].tangentOut);
	}

	CreateCurve(curve, lookupTable, nSteps, iElement);

	return bSuccess;
}

void CreateCurve(const Curve::C_HermiteCurve& curve, LookupTable& lookupTable, S32 nSteps, S32 iElement)
{
	F32 fSize = (F32)(nSteps - 1);

	// if curve isn't valid then add some default values
	if(!curve.Valid())
	{
		for(S32 i = 0; i < nSteps; ++i)
		{
			F32 value = 1.f;
			if(iElement == -1)
				lookupTable.table[i] = MathsStorage::Float4A16(value, value, value, value);
			else
				lookupTable.table[i].v[iElement] = value;
		}
		lookupTable.vSize = MathsStorage::Float4A16(fSize, fSize, fSize, fSize);
	}
	else
	{
		// curve is ok so evaluate
		Ut::Vector<F32> table;
		curve.EvaluateTable(table, nSteps);

		// copy tables into correct place in vector
		for(S32 i = 0; i < nSteps; ++i)
		{
			F32 value = table[i];
			if(iElement == -1)
				lookupTable.table[i] = MathsStorage::Float4A16(value, value, value, value);
			else
				lookupTable.table[i].v[iElement] = value;
		}
		
		lookupTable.vSize = MathsStorage::Float4A16(fSize, fSize, fSize, fSize);
	}
}

}; // namespace XtInterface
}; // namespace DynamicEffect
