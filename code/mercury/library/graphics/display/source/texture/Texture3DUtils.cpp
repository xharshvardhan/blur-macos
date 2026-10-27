#include "stdafx.h"
#include "Display/Texture/Texture3DUtils.h"
#include "Display/Device/Device.h"
#include "Display/Utils/File.h"
#include "Display/Texture/DDS/DDSTextureUtils.h"
#include <System/Misc/StringUtil.h>
#if defined(PLATFORM_MACOS) // [Blur2PC] Reuse the bounded DDS decoder and native staging resources.
#include "../../../display2/source/device/metal/MetalImageLoading.h"
#include <display2/utils/Utils.h>
#include <memory>
#endif

#ifdef CFG_ALLOW_OPTIMIZE_OFF
//#pragma optimize( "", off ) // CFG_ALLOW_OPTIMIZE_OFF 
#endif

namespace Display
{


bool UploadTexture3D(C_Texture3D* pTexture, C_TextureParams* pTextureParams)
{

#if defined(PLATFORM_MACOS) // [Blur2PC] Validate the complete payload before mapping any native mip.
    if(!pTexture||!pTextureParams||!pTexture->IsCreated()||!pTextureParams->m_pMipData||pTextureParams->m_MipHead>pTextureParams->m_MipTail||pTextureParams->m_MipTail>=pTexture->GetLevels()||pTextureParams->m_MipOffset>pTextureParams->m_MipSize)return false;
    const auto format=pTexture->GetFormat();U64 required=pTextureParams->m_MipOffset;
    if(pTexture->GetType()!=Display2::E_TextureType_3D)return false;
    for(U32 level=pTextureParams->m_MipHead;level<=pTextureParams->m_MipTail;++level)required+=Display2::GetTexture3DSize(pTexture->GetWidth(level),pTexture->GetHeight(level),pTexture->GetDepth(level),0,format);
    if(required>pTextureParams->m_MipSize)return false;
    const U8* data=pTextureParams->m_pMipData+pTextureParams->m_MipOffset;
    for(U32 level=pTextureParams->m_MipHead;level<=pTextureParams->m_MipTail;++level){
        const U32 row=Display2::GetTexturePitchFromWidth(pTexture->GetWidth(level),0,format),height=pTexture->GetHeight(level),depth=pTexture->GetDepth(level);
        auto lock=pTexture->LockBox3D(level);if(!lock.pBits)return false;
        if(lock.rowPitch<row||U64(lock.slicePitch)<U64(lock.rowPitch)*height){pTexture->UnlockBox3D(level);return false;}
        for(U32 z=0;z<depth;++z)for(U32 y=0;y<height;++y)memcpy(lock.pBits+U64(z)*lock.slicePitch+U64(y)*lock.rowPitch,data+(U64(z)*height+y)*row,row);
        pTexture->UnlockBox3D(level);data+=U64(row)*height*depth;
    }
    return true;
#else

	BIZ_ASSERT(pTexture);
	
	if(!pTexture->IsCreated())
		return false;
	
	BIZ_ASSERT(pTextureParams);
	BIZ_ASSERT(pTextureParams->m_pMipData);
	BIZ_ASSERT(pTextureParams->m_MipSize > 0);
	BIZ_ASSERT(pTextureParams->m_MipOffset < pTextureParams->m_MipSize);

	U8* pData = &pTextureParams->m_pMipData[pTextureParams->m_MipOffset];

	U32 width = pTextureParams->m_Width;
	U32 height = pTextureParams->m_Height;
	U32 depth = pTextureParams->m_Depth;

	const Display2::E_TextureFormat format = GetTextureFormatFromFormat(pTextureParams->m_Format);

	U32 slicePitch = Display2::GetTextureSlicePitch(width, height, pTextureParams->m_Levels, pTextureParams->m_Flags, format);
	
	for(U32 level=pTextureParams->m_MipHead; level<=pTextureParams->m_MipTail; level++)
	{
		BIZ_ASSERT(pData <= pTextureParams->m_pMipData+pTextureParams->m_MipSize);

		C_LockedBox lockedBox = pTexture->LockBox3D(level);

		if(!lockedBox.GetBits())
		{
			LOGERROR(g_Log, "Failed to lock 3d texture level %i\n", level);
			return false;
		}

		U32 rowPitch = Display2::GetTextureRowPitch(width, pTextureParams->m_Flags, format);
		U32 size = Display2::GetTexture3DSize(width, height, depth, pTextureParams->m_Flags, format);
		
		BIZ_ASSERT(pData+size <= pTextureParams->m_pMipData+pTextureParams->m_MipSize);

		memcpy(lockedBox.GetBits(), pData, size);
		
		pData += size;
		
		pTexture->UnlockBox3D(level);
		
		if(width > 1)
			width >>= 1;
		if(height > 1)
			height >>= 1;
	}

	BIZ_ASSERT(pData == pTextureParams->m_pMipData+pTextureParams->m_MipSize);

	return true;
#endif

}


U32 GetTexture3DSize(const U32 width, const U32 height, const U32 depth, const U32 flags, const E_Format format)
{
	return Display2::GetTexture3DSize(width, height, depth, flags, GetTextureFormatFromFormat(format));
}

U32 GetTexture3DSize(U32 width, U32 height, const U32 depth, const U32 levels, const U32 flags, const E_Format format)
{
	return Display2::GetTexture3DSize(width, height, depth, levels, flags, GetTextureFormatFromFormat(format));
}


bool CreateTexture3DFromFile( Util::C_String fileName, C_Texture3D* pTexture, const U32 flags)
{
#if defined(PLATFORM_MACOS) // [Blur2PC] File payload stays owned until every native row/slice upload finishes.
    if(!pTexture||pTexture->IsCreated()||!DeviceExists()||!Device()->IsCreated()||fileName.Length()<4)return false;
    Display2::C_TextureParams decoded;
    if(!Display2::Metal::LoadDDS(fileName,&decoded,4))return false;
    std::unique_ptr<U8[]> bytes(decoded.m_pFileData);C_TextureParams params;
    params.m_Width=decoded.m_Width;params.m_Height=decoded.m_Height;params.m_Depth=decoded.m_Depth;params.m_Levels=decoded.m_Levels;params.m_Format=static_cast<E_Format>(decoded.m_Format);params.m_MipTail=decoded.m_Levels-1;
    params.m_pMipData=bytes.get();params.m_MipSize=decoded.m_FileSize;params.m_MipOffset=decoded.m_MipOffset;
    params.m_Flags=flags;
    params.m_Flags|=E_Format_Linear|E_Format_Raw;
    if(!Device()->CreateTexture3D(params.m_Width,params.m_Height,params.m_Depth,params.m_Levels,params.m_Flags,params.m_Format,pTexture))return false;
    try{if(UploadTexture3D(pTexture,&params))return true;}catch(const std::exception& error){LOGERROR(g_Log,"Native DDS upload failed: %s",error.what());}
    Device()->DestroyTexture3D(pTexture);return false;
#else

	if(fileName == "")
	{
		LOGERROR(g_Log, "No file name specified when creating 3d texture from file\n");
		return false;
	}

	fileName.ConvertToLower();

#if 0
	return D3DXCreateTexture3DFromFile(fileName, pTexture);
#endif

	C_TextureParams params;
	
	if(fileName.Search(".dds") != -1)
	{
		if(!DDS::LoadTextureFromFile(fileName, &params))
			return false;
	}
	else
	{
		LOGERROR(g_Log, "Unsupported 3d texture file extension '%s'\n", fileName.Chars());
		return false;
	}

	params.m_Name = fileName;

	#if defined(PLATFORM_PS3) && !defined(CFG_GOLD)
	if((params.m_Width & (params.m_Width-1)) || (params.m_Height & (params.m_Height-1)) || (params.m_Depth & (params.m_Depth-1)))
	{
		LOGERROR(g_Log, "Texture '%s' is not power of 2 (%ix%ix%i)\n", fileName.Chars(), params.m_Width, params.m_Height, params.m_Depth);
	}
	#endif

	params.m_Flags = flags;
	params.m_Flags |= E_Format_Linear;
	params.m_Flags |= E_Format_Raw;
	if(!Device()->CreateTexture3D(params.m_Width, params.m_Height, params.m_Depth, params.m_Levels, params.m_Flags, params.m_Format, pTexture))
	{
		SAFE_DELETE_ARY(params.m_pMipData);
		return false;
	}
	
	params.m_MipHead = 0;
	params.m_MipTail = params.m_Levels-1;

	if(!UploadTexture3D(pTexture, &params))
	{
		Device()->DestroyTexture3D(pTexture);
		SAFE_DELETE_ARY(params.m_pMipData);
		LOGERROR(g_Log, "Failed to upload the 3d texture '%s'\n", fileName.Chars());
		return false;
	}
	
	#if TRACK_DISPLAY2_RESOURCES
	Display2::Device()->SetTextureName(pTexture, fileName);
	#endif

	SAFE_DELETE_ARY(params.m_pMipData);

	return true;
#endif

}

}; // namespace Display
