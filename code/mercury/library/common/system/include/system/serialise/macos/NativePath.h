// [Blur2PC][macOS] Share original-case path resolution across sync/async devices.
#pragma once
#include <system/serialise/FilePath.h>
#include <filesystem>
namespace Sys { std::filesystem::path NativeResolvePath(const C_FilePath& path); }
