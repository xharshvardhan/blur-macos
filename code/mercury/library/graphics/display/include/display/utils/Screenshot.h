#ifndef DISPLAY_UTILS_SCREENSHOT_H
#define DISPLAY_UTILS_SCREENSHOT_H

#ifdef _MSC_VER
#pragma once
#endif

#include "Display/Surface/Surface.h"

namespace Display
{

void TakeScreenshot(const Sys::C_FilePath& fileName, const C_Surface* pSurface);
void TakeScreenshot(const Sys::C_FilePath& fileName);

// [Blur1PCDX11] Capture the swap-chain image during automated, minimized runs.
#if defined(DEVICE_PCD3D11) // [Blur2PC][macOS] D3D capture types belong to their backend.
void TakeD3D11BackBufferScreenshot(const char* fileName, ID3D11Texture2D* source, bool scRGB = false, F32 paperWhiteScale = 1.0f);
#endif

}; // namespace Display

#endif // DISPLAY_UTILS_SCREENSHOT_H
