// [Blur2PC][macOS] Entry point for the actual original game application.
// This delegates startup, game layers and simulation to AmaxApplicationMain.
#include <core/Core.h>
#include <core/containers.h>
#include <system/serialise/FilePath.h>
#include <framework/application/CoreSystems.h>
#include <framework/application/ApplicationParams.h>
#include <framework/application/macos/RunDiagnostics.h>
#include <display2/Utils/Utils.h>
#include "NativeGameLaunch.h"
#import <Cocoa/Cocoa.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <exception>
#include <cstdio>
#include <cmath>
int AmaxApplicationMain(Framework::C_CoreSystemsParams&,Framework::C_ApplicationParams&);
namespace {
// [Blur2PC] Keep the selected existing data outside the app bundle and use the
// native preferences store; the launcher never copies or changes game files.
NSString* const kGameDataRootPreference = @"BlurGameDataRoot";

std::filesystem::path validateDataRoot(const std::filesystem::path& selection) {
    namespace fs = std::filesystem;
    std::error_code error;
    const auto root = fs::canonical(selection, error);
    if (error || !fs::is_directory(root))
        throw std::runtime_error("The selected folder is unavailable or is not a directory.");
    auto requireFile = [&](const fs::path& relative) {
        const auto file = root / relative;
        if (!fs::is_regular_file(file) || fs::file_size(file) == 0)
            throw std::runtime_error("The selected folder is missing a required, nonempty game file: " + relative.string());
    };
    requireFile("cache/root.pak");
    requireFile("cache/gamedata.pak");
    requireFile("UI/movies/atvi.bik");
    requireFile("UI/movies/BizarreSplash.bik");
    if (!fs::is_directory(root / "levels"))
        throw std::runtime_error("The selected folder is missing the levels directory.");
    bool hasLevel = false;
    for (const auto& entry : fs::directory_iterator(root / "levels")) {
        if (!entry.is_directory()) continue;
        bool complete = true;
        for (const char* name : {"Level.level", "Level.levelrender", "Level.levelphysics", "Level.levelstream"}) {
            const auto file = entry.path() / name;
            if (!fs::is_regular_file(file) || fs::file_size(file) == 0) { complete = false; break; }
        }
        if (complete) { hasLevel = true; break; }
    }
    if (!hasLevel)
        throw std::runtime_error("The levels directory contains no complete level (Level.level, Level.levelrender, Level.levelphysics and Level.levelstream).");
    return root;
}

bool showDataError(const std::string& detail) {
    NSAlert* alert = [NSAlert new];
    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = @"Choose the complete Blur game folder";
    alert.informativeText = [NSString stringWithFormat:@"%s\n\nSelect the folder containing cache, levels and UI, rather than one of those subfolders. The existing game data will only be read.", detail.c_str()];
    [alert addButtonWithTitle:@"Choose Another Folder…"];
    [alert addButtonWithTitle:@"Quit"];
    return [alert runModal] == NSAlertFirstButtonReturn;
}

bool chooseManualDataRoot(std::filesystem::path& root) {
    NSApplication* app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [app activateIgnoringOtherApps:YES];
    NSUserDefaults* preferences = [NSUserDefaults standardUserDefaults];
    id saved = [preferences objectForKey:kGameDataRootPreference];
    NSString* previous = [saved isKindOfClass:[NSString class]] ? saved : nil;
    if (previous.length) {
        try { root = validateDataRoot(previous.fileSystemRepresentation); return true; }
        catch (const std::exception& error) {
            if (!showDataError(std::string("The remembered game folder cannot be used.\n") + error.what())) return false;
        }
    }
    for (;;) {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.title = @"Choose Blur game data";
        panel.prompt = @"Use Game Folder";
        panel.message = @"Choose the complete Blur folder containing cache, levels and UI. Game files are not included in this app.";
        panel.canChooseDirectories = YES;
        panel.canChooseFiles = NO;
        panel.canCreateDirectories = NO;
        panel.allowsMultipleSelection = NO;
        if (previous.length) panel.directoryURL = [NSURL fileURLWithPath:previous isDirectory:YES];
        if ([panel runModal] != NSModalResponseOK) return false;
        previous = panel.URL.path;
        try { root = validateDataRoot(panel.URL.fileSystemRepresentation); }
        catch (const std::exception& error) { if (showDataError(error.what())) continue; return false; }
        [preferences setObject:[NSString stringWithUTF8String:root.c_str()] forKey:kGameDataRootPreference];
        return true;
    }
}
}
int main(int argc,char** argv) {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    std::setvbuf(stderr,nullptr,_IONBF,0);
    @autoreleasepool {
        try {
            if(argc==2&&std::strcmp(argv[1],"--help")==0){
                std::cout<<"Blur native development game: --data-root <complete PC game-data directory>\n"
                    <<"Optional: --capture <PNG path> --capture-after <seconds> --quit-after <seconds>\n"
                    <<"Launch without arguments to choose and remember an existing game-data folder.\n";
                return 0;
            }
            std::string dataRoot,capturePath;double captureAfter=-1,quitAfter=-1;
            auto seconds=[](const char* text){std::size_t used=0;const std::string value(text);const double number=std::stod(value,&used);
                if(used!=value.size()||!std::isfinite(number)||number<0)throw std::invalid_argument("Diagnostic seconds must be a finite nonnegative number");return number;};
            for(int i=1;i<argc;++i){
                const std::string option(argv[i]);
                if(i+1>=argc)throw std::invalid_argument("Missing value for "+option);
                if(option=="--data-root")dataRoot=argv[++i];
                else if(option=="--capture")capturePath=argv[++i];
                else if(option=="--capture-after")captureAfter=seconds(argv[++i]);
                else if(option=="--quit-after")quitAfter=seconds(argv[++i]);
                else throw std::invalid_argument("Unknown option "+option);
            }
            std::filesystem::path root;
            if(argc==1){if(!chooseManualDataRoot(root))return 0;AmaxMac::SetManualLaunch(true);}
            else if(dataRoot.empty()){
                std::cerr<<"Provide --data-root <complete PC game-data directory>.\n";return 2;
            }
            if(capturePath.empty()&&captureAfter>=0)throw std::invalid_argument("--capture-after requires --capture");
            if(!capturePath.empty()){capturePath=std::filesystem::absolute(capturePath).string();if(captureAfter<0)captureAfter=0;}
            if(argc!=1)root=validateDataRoot(dataRoot);
            NSString* resources=[[NSBundle mainBundle] resourcePath];
            if(!resources)throw std::runtime_error("Native app resource directory is missing");
            Display2::SetNativeShaderResourceRoot(resources.UTF8String);
            Framework::Mac::ConfigureRunDiagnostics(capturePath,captureAfter,quitAfter);
            Framework::C_CoreSystemsParams systems;
            systems.m_Name="Amax";systems.m_WorkingDir=Sys::C_FilePath(root.string().c_str());
            Framework::C_ApplicationParams application;
            return AmaxApplicationMain(systems,application);
        }catch(const std::exception& error){
            std::cerr<<"Native game startup failed: "<<error.what()<<'\n';
            if(argc==1){
                NSAlert* alert=[NSAlert new];alert.alertStyle=NSAlertStyleCritical;
                alert.messageText=@"Blur could not start";
                alert.informativeText=[NSString stringWithFormat:@"%s",error.what()];
                [alert addButtonWithTitle:@"Quit"];[alert runModal];
            }
            return 1;
        }
    }
}
