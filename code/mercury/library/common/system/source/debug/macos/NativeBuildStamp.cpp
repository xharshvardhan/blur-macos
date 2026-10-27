// [Blur2PC][macOS] Original build identity API over generated native metadata.
#include <system/debug/BuildStamp.h>
#include <NativeBuildStampData.h>
namespace Debug {
bool GetServerBuild() {
#ifdef SERVER_BUILD
    return true;
#else
    return false;
#endif
}
const char* GetLine(int line) {
    return line >= 0 && line < int(sizeof(nativeBuildStampFields) / sizeof(*nativeBuildStampFields))
         ? nativeBuildStampFields[line] : "Unknown";
}
const char* GetBuildTime() { return GetLine(0); }
const char* GetBuildDate() { return GetLine(1); }
const char* GetBuildUser() { return GetLine(2); }
const char* GetBuildPc() { return GetLine(3); }
const char* GetBuildVersion() { return GetLine(4); }
const char* GetBuildRevision() { return GetLine(6); }
}
