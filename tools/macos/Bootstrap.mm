// [Blur2PC][macOS] Native development bootstrap, deliberately not a game menu.
#import <Cocoa/Cocoa.h>
#include <display/device/metal/MetalBootstrap.h>
#include <pakfs/macos/NativePakArchive.h>
#include <iostream>

@interface BlurBootstrapDelegate : NSObject <NSApplicationDelegate>
@property(strong) NSWindow* window;
@property(strong) NSTextView* output;
@end

@implementation BlurBootstrapDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 820, 480)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Blur — native Mac bootstrap (not yet playable)";
    self.window.minSize = NSMakeSize(640, 360);
    NSView* content = self.window.contentView;
    NSTextField* heading = [NSTextField labelWithString:@"Native Mac port · platform bring-up"];
    heading.font = [NSFont systemFontOfSize:24 weight:NSFontWeightSemibold];
    heading.frame = NSMakeRect(24, 420, 770, 34);
    heading.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:heading];
    NSTextField* status = [NSTextField wrappingLabelWithString:
        @"Apple Silicon / Cocoa / Metal. This build validates native rendering and game archive access. The full engine, gameplay and multiplayer are still being ported."];
    status.frame = NSMakeRect(24, 355, 770, 54);
    status.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:status];
    NSButton* choose = [NSButton buttonWithTitle:@"Choose existing Blur game folder…" target:self action:@selector(chooseFolder:)];
    choose.frame = NSMakeRect(24, 304, 320, 34);
    choose.autoresizingMask = NSViewMinYMargin;
    [content addSubview:choose];
    NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(24, 24, 772, 260)];
    scroll.hasVerticalScroller = YES;
    scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.output = [[NSTextView alloc] initWithFrame:scroll.bounds];
    self.output.editable = NO;
    self.output.font = [NSFont monospacedSystemFontOfSize:13 weight:NSFontWeightRegular];
    self.output.textContainerInset = NSMakeSize(12, 12);
    self.output.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    scroll.documentView = self.output;
    [content addSubview:scroll];
    self.output.string = @"Testing native Metal rendering…";
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [self runProbe];
}
- (void)runProbe {
    try {
        auto result = Blur::Mac::runMetalProbe();
        self.output.string = [NSString stringWithFormat:
            @"Metal indexed draw and GPU readback: PASS\nDevice: %s\n\nChoose the original installation folder to inspect cache/*.pak.\nNo game files are bundled, downloaded or modified.\n\nArchive compatibility with retail files has not yet been tested.\nA successful archive check will not enable gameplay in this bootstrap.", result.device.c_str()];
    } catch (const std::exception& error) {
        self.output.string = [NSString stringWithFormat:@"Metal bring-up failed:\n%s", error.what()];
    }
}
- (void)chooseFolder:(id)sender {
    (void)sender;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = NO;
    panel.allowsMultipleSelection = NO;
    panel.message = @"Choose the Blur installation containing the cache folder.";
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) return;
        try {
            auto report = Blur::Mac::inspectDataRoot(panel.URL.fileSystemRepresentation);
            self.output.string = [NSString stringWithUTF8String:report.c_str()];
        } catch (const std::exception& error) {
            self.output.string = [NSString stringWithFormat:@"Archive inspection failed:\n%s", error.what()];
        }
    }];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}
@end

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        // Exercise the bundle executable without opening or focusing any UI.
        if (argc == 2 && std::string(argv[1]) == "--self-test") {
            try {
                auto result = Blur::Mac::runMetalProbe();
                std::cout << "Native bundle Metal/readback PASS: " << result.device << '\n';
                return 0;
            } catch (const std::exception& error) {
                std::cerr << error.what() << '\n';
                return 1;
            }
        }
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        BlurBootstrapDelegate* delegate = [BlurBootstrapDelegate new];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
