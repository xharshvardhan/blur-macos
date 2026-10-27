// [Blur2PC][macOS] Bounded native image decoding for original Display2 file helpers.
#pragma once
#include <core/containers/String.h>
namespace Display2 {struct C_TextureParams;struct C_Texture;namespace Metal {
// [Blur2PC] Private native DDS2D transaction; other image/type helpers retain their original paths.
bool ReadDDS2DFileData(const Util::C_String& file,U32& size,U8** bytes);
bool CreateDDS2D(const Util::C_String& file,C_Texture* texture);
bool LoadDDS(const Util::C_String& file,C_TextureParams* params);
bool LoadDDS(const Util::C_String& file,C_TextureParams* params,U32 textureType);
bool LoadTGA(const Util::C_String& file,C_TextureParams* params);
bool LoadBMP(const Util::C_String& file,C_TextureParams* params);
}}
