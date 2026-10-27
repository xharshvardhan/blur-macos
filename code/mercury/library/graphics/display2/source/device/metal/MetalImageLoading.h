// [Blur2PC][macOS] Bounded native image decoding for original Display2 file helpers.
#pragma once
#include <core/containers/String.h>
namespace Display2 {struct C_TextureParams;namespace Metal {
bool LoadDDS(const Util::C_String& file,C_TextureParams* params);
bool LoadDDS(const Util::C_String& file,C_TextureParams* params,U32 textureType);
bool LoadTGA(const Util::C_String& file,C_TextureParams* params);
bool LoadBMP(const Util::C_String& file,C_TextureParams* params);
}}
