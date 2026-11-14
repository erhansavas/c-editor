#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/diagnostics.hpp"
#include "core/encoding.hpp"

namespace cedit {

struct CompileRequest {
    std::string source_path;  // the user's file; diagnostics name it by its file name
    std::string text;         // current UTF-8 editor text
    Encoding encoding;        // how the file is stored: a Windows-1254 file is compiled from its own bytes
    std::string compat_dir;   // the compat folder of the app bundle: the headers programs see and the runtime's source
    std::string work_dir;     // per-document scratch directory, created if missing
    std::string cache_dir;    // keeps the runtime built for this Mac between runs, created if missing
};

struct CompileResult {
    bool ok = false;
    bool no_compiler = false;  // Apple's developer tools are not installed: nothing was built
    std::string binary;
    std::string log;  // compiler output with Turkish explanations, followed by a summary
    std::vector<Diagnostic> diagnostics;
};

// Starts the log lines that explain the compiler message above them.
constexpr std::string_view hint_prefix = "  ➜ ";

// Compiles one C (or, by extension, C++) file the way code written on Windows expects: Windows console
// idioms from the compat headers and the linked runtime, gnu dialects, GCC's leniency, no optimisation.
// The compiler is this Mac's own, from Apple's developer tools; it also builds the runtime, once.
CompileResult compile(const CompileRequest& request);

// The compiler in `developer_dir`, as xcode-select -p prints it: the Command Line Tools' or Xcode's.
std::optional<std::string> compiler_in(const std::string& developer_dir);

// Opens Apple's installer for the Command Line Tools.
void request_developer_tools();

}  // namespace cedit
