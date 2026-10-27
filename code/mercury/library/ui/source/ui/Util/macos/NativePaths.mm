// [Blur2PC] Query the public macOS application-support location without changing host settings.
#include <ui/Util/NativePaths.h>
#import <Foundation/Foundation.h>
#include <stdexcept>
namespace UI { namespace NativePaths {
std::string ApplicationSupport() {
    @autoreleasepool {
        NSString* path=NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,NSUserDomainMask,YES).firstObject;
        const char* utf8=path.fileSystemRepresentation;
        if(!utf8)throw std::runtime_error("Native application-support directory is unavailable");
        return utf8;
    }
}
} }
