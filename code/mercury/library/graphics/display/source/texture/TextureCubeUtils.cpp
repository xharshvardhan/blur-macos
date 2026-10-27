#include "stdafx.h"
#include "Display/Texture/TextureCubeUtils.h"
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


bool UploadTextureCube(C_TextureCube* pTexture, C_TextureParams* pTextureParams)
{

#if defined(PLATFORM_MACOS) // [Blur2PC] Validate the complete payload before mapping any native mip.
    if(!pTexture||!pTextureParams||!pTexture->IsCreated()||!pTextureParams->m_pMipData||pTextureParams->m_MipHead>pTextureParams->m_MipTail||pTextureParams->m_MipTail>=pTexture->GetLevels()||pTextureParams->m_MipOffset>pTextureParams->m_MipSize)return false;
    const auto format=pTexture->GetFormat();U64 required=pTextureParams->m_MipOffset;
    if(pTexture->GetType()!=Display2::E_TextureType_Cube)return false;
    for(U32 level=pTextureParams->m_MipHead;level<=pTextureParams->m_MipTail;++level)required+=U64(Display2::GetTexture2DSize(pTexture->GetWidth(level),pTexture->GetHeight(level),0,format))*6;
    if(required>pTextureParams->m_MipSize)return false;
    const U8* data=pTextureParams->m_pMipData+pTextureParams->m_MipOffset;
    for(U32 face=0;face<6;++face)for(U32 level=pTextureParams->m_MipHead;level<=pTextureParams->m_MipTail;++level){
        const U32 row=Display2::GetTexturePitchFromWidth(pTexture->GetWidth(level),0,format),bytes=Display2::GetTexture2DSize(pTexture->GetWidth(level),pTexture->GetHeight(level),0,format);
        auto lock=pTexture->LockRectCube(face,level);if(!lock.pBits)return false;
        if(lock.pitch<row){pTexture->UnlockRectCube(face,level);return false;}
        for(U32 y=0;y<bytes/row;++y)memcpy(lock.pBits+U64(y)*lock.pitch,data+U64(y)*row,row);
        pTexture->UnlockRectCube(face,level);data+=bytes;
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

	const Display2::E_TextureFormat format = GetTextureFormatFromFormat(pTextureParams->m_Format);

	for(U32 face=0; face<E_CubeTextureFace_Count; face++)
	{
		U32 width = pTextureParams->m_Width;

		for(U32 level=pTextureParams->m_MipHead; level<=pTextureParams->m_MipTail; level++)
		{
			BIZ_ASSERT(pData <= pTextureParams->m_pMipData+pTextureParams->m_MipSize);

			C_LockedRect lockedRect = pTexture->LockRectCube((Display2::E_CubeMapFace)face, level);

			if(!lockedRect.GetBits())
			{
				LOGERROR(g_Log, "Failed to lock cube texture face %i level %i\n", face, level);
				return false;
			}

			U32 pitch = Display2::GetTexturePitchFromWidth(width, pTextureParams->m_Flags, format);
			U32 size = Display2::GetTexture2DSize(width, width, pTextureParams->m_Flags, format);

			BIZ_ASSERT(pData+size <= pTextureParams->m_pMipData+pTextureParams->m_MipSize);
			
			memcpy(lockedRect.GetBits(), pData, size);

			pData += size;

			pTexture->UnlockRectCube((Display2::E_CubeMapFace)face, level);
			
			if(width > 1)
				width >>= 1;
		}
	}

	BIZ_ASSERT(pData == pTextureParams->m_pMipData+pTextureParams->m_MipSize);

	return true;
#endif

}

U32 GetTextureCubeSize(const U32 width, const U32 flags, const E_Format format)
{
	return Display2::GetTextureCubeSize(width, flags, GetTextureFormatFromFormat(format));
}

U32 GetTextureCubeSize(U32 width, const U32 levels, const U32 flags, const E_Format format)
{
	return Display2::GetTextureCubeSize(width, levels, flags, GetTextureFormatFromFormat(format));
}


bool CreateTextureCubeFromFile(Util::C_String fileName, C_TextureCube* pTexture)
{
#if defined(PLATFORM_MACOS) // [Blur2PC] File payload stays owned until every native row/slice upload finishes.
    if(!pTexture||pTexture->IsCreated()||!DeviceExists()||!Device()->IsCreated()||fileName.Length()<4)return false;
    Display2::C_TextureParams decoded;
    if(!Display2::Metal::LoadDDS(fileName,&decoded,5))return false;
    std::unique_ptr<U8[]> bytes(decoded.m_pFileData);C_TextureParams params;
    params.m_Width=decoded.m_Width;params.m_Height=decoded.m_Height;params.m_Depth=decoded.m_Depth;params.m_Levels=decoded.m_Levels;params.m_Format=static_cast<E_Format>(decoded.m_Format);params.m_MipTail=decoded.m_Levels-1;
    params.m_pMipData=bytes.get();params.m_MipSize=decoded.m_FileSize;params.m_MipOffset=decoded.m_MipOffset;
    params.m_Flags|=E_Format_Linear|E_Format_Raw;
    if(!Device()->CreateTextureCube(params.m_Width,params.m_Levels,params.m_Flags,params.m_Format,pTexture))return false;
    try{if(UploadTextureCube(pTexture,&params))return true;}catch(const std::exception& error){LOGERROR(g_Log,"Native DDS upload failed: %s",error.what());}
    Device()->DestroyTextureCube(pTexture);return false;
#else

	if(fileName == "")
	{
		LOGERROR(g_Log, "No file name specified when creating cube texture from file\n");
		return false;
	}

	fileName.ConvertToLower();

#if 0
	return D3DXCreateTextureCubeFromFile(fileName, pTexture);
#endif

	C_TextureParams params;
	
	if(fileName.Search(".dds") != -1)
	{
		if(!DDS::LoadTextureFromFile(fileName, &params))
			return false;
	}
	else
	{
		LOGERROR(g_Log, "Unsupported cube texture file extension '%s'\n", fileName.Chars());
		return false;
	}

	params.m_Name = fileName;

	#if defined(PLATFORM_PS3) && !defined(CFG_GOLD)
	if(params.m_Width & (params.m_Width-1))
	{
		LOGERROR(g_Log, "Cube texture'%s' is not power of 2 (%i)\n", fileName.Chars(), params.m_Width);
	}
	#endif

	params.m_Flags |= E_Format_Linear;
	params.m_Flags |= E_Format_Raw;

	if(!Device()->CreateTextureCube(params.m_Width, params.m_Levels, params.m_Flags, params.m_Format, pTexture))
	{
		SAFE_DELETE_ARY(params.m_pMipData);
		return false;
	}

	params.m_MipHead = 0;
	params.m_MipTail = params.m_Levels-1;

	if(!UploadTextureCube(pTexture, &params))
	{
		Device()->DestroyTextureCube(pTexture);
		SAFE_DELETE_ARY(params.m_pMipData);
		LOGERROR(g_Log, "Failed to upload the cube texture '%s'\n", fileName.Chars());
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
