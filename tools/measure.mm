// `./build.sh measure` builds this as the app is released and runs it: what a keystroke costs in the real editor
// view, from the key event to the redrawn pixels, and how long ⌘R takes to build a program. Run it with nothing else
// busy.
#import <AppKit/AppKit.h>

#include <mach/mach_time.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#import "platform/macos/DocumentWindow.h"
#import "platform/macos/EditorView.h"
#include "posix/compile.hpp"
#include "posix/files.hpp"

namespace {

double milliseconds() {
    static mach_timebase_info_data_t base;
    if (base.denom == 0) mach_timebase_info(&base);
    return static_cast<double>(mach_absolute_time()) * base.numer / base.denom / 1e6;
}

double percentile(std::vector<double> samples, std::size_t percent) {
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() * percent / 100];
}

// Types into the middle of a file of `lines` lines, a Turkish letter every other key, redrawing after each.
void keystrokes(NSString* folder, std::size_t lines) {
    std::string text = "int main() {\n";
    for (std::size_t i = 0; i < lines; ++i) text += "    if (toplam > 10) { printf(\"Merhaba %d\\n\", toplam); } /* yorum */\n";
    text += "}\n";
    NSString* path = [folder stringByAppendingPathComponent:[NSString stringWithFormat:@"%zu.c", lines]];
    cedit::write_file_atomic(path.fileSystemRepresentation, text);
    CEDocumentWindow* document = [CEDocumentWindow openPath:path];
    NSWindow* window = document.window;
    window.alphaValue = 0;
    [window setFrame:NSMakeRect(80, 60, 800, 700) display:YES];
    [window.contentView.superview displayIfNeeded];
    CEEditorView* view = static_cast<CEEditorView*>(window.firstResponder);
    view.editor->set_caret(text.size() / 2, false);
    NSBitmapImageRep* pixels = [view bitmapImageRepForCachingDisplayInRect:view.visibleRect];
    std::vector<double> samples;
    for (int i = 0; i < 200; ++i) {
        @autoreleasepool {
            NSString* letter = i % 2 ? @"ş" : @"a";
            NSEvent* key = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0
                                        windowNumber:window.windowNumber context:nil characters:letter
                         charactersIgnoringModifiers:letter isARepeat:NO keyCode:0];
            const double start = milliseconds();
            [window sendEvent:key];
            [view displayIfNeeded];
            [view cacheDisplayInRect:view.visibleRect toBitmapImageRep:pixels];
            samples.push_back(milliseconds() - start);
        }
    }
    std::printf("keystroke, %6zu lines: median %.3f ms, 95th percentile %.3f ms\n", lines, percentile(samples, 50),
                percentile(samples, 95));
    [document endWork];
    [window close];
}

// The first build also builds the runtime; the ones after it are what every later ⌘R costs.
void builds(const std::string& compat, const std::string& folder, const std::string& name, const std::string& code) {
    const std::string source = folder + "/" + name;
    cedit::write_file_atomic(source, code);
    const cedit::LoadedText loaded = cedit::load_text(code);
    const std::string cache = folder + "/cache-" + name;
    [NSFileManager.defaultManager removeItemAtPath:[NSString stringWithUTF8String:cache.c_str()] error:nil];
    const cedit::CompileRequest request{source, loaded.text, loaded.format.encoding, compat, folder + "/work", cache};
    std::vector<double> samples;
    double first = 0;
    for (int i = 0; i < 11; ++i) {
        const double start = milliseconds();
        const cedit::CompileResult result = cedit::compile(request);
        if (!result.ok) std::printf("%s", result.log.c_str());
        (i == 0 ? first : samples.emplace_back()) = milliseconds() - start;
    }
    std::printf("build, %-11s: first %.0f ms, then median %.0f ms\n", name.c_str(), first, percentile(samples, 50));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: measure <compat-dir> <scratch-dir>\n");
        return 2;
    }
    @autoreleasepool {
        const std::string folder = std::string(argv[2]) + "/measure";
        ::mkdir(folder.c_str(), 0700);
        NSApplication.sharedApplication.activationPolicy = NSApplicationActivationPolicyAccessory;
        [NSApp finishLaunching];
        NSString* files = [NSString stringWithUTF8String:folder.c_str()];
        for (const std::size_t lines : {100u, 2000u, 20000u}) keystrokes(files, lines);
        builds(argv[1], folder, "merhaba.c",
               "#include <stdio.h>\r\n#include <locale.h>\r\nint main()\r\n{\r\n    setlocale(LC_ALL, \"Turkish\");\r\n"
               "    printf(\"Merhaba d\xFCnya\\n\");\r\n    system(\"pause\");\r\n    return 0;\r\n}\r\n");
        builds(argv[1], folder, "merhaba.cpp", "#include <iostream>\nusing namespace std;\nint main() {\n    cout << \"Merhaba\" << endl;\n    return 0;\n}\n");
    }
    return 0;
}
