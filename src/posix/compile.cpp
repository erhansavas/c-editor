#include "compile.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <expected>
#include <string_view>

#include "core/utf8.hpp"
#include "posix/files.hpp"
#include "posix/process.hpp"

namespace cedit {
namespace {

// Programs are built for the processor this app runs as, so they run the way it does.
#if defined(__arm64__)
constexpr std::string_view architecture = "arm64";
#else
constexpr std::string_view architecture = "x86_64";
#endif

std::string_view file_name(std::string_view path) {
    const std::size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

// By its extension, in any case, as Windows file names come: only ".C" alone means C++ where ".c" does not.
bool is_cpp(std::string_view path) {
    if (path.ends_with(".C")) return true;
    std::string extension(path.substr(std::min(path.rfind('.'), path.size())));
    for (char& c : extension) c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    for (const std::string_view known : {".cpp", ".cc", ".cxx", ".cp", ".c++"})
        if (extension == known) return true;
    return false;
}

// "hata yok", "2 hata".
std::string count(std::size_t n, std::string_view noun) {
    return n == 0 ? std::string(noun) + " yok" : std::to_string(n) + " " + std::string(noun);
}

// Line `index` (0-based) of `text`.
std::string_view line_of(std::string_view text, std::size_t index) {
    std::size_t start = 0;
    for (; index > 0; --index) {
        const std::size_t newline = text.find('\n', start);
        if (newline == std::string_view::npos) return {};
        start = newline + 1;
    }
    return text.substr(start, text.find('\n', start) - start);
}

// Apple's compiler, where the active developer tools keep it; without them /usr/bin/clang can only ask for them.
std::optional<std::string> installed_compiler() {
    const std::optional<Captured> selected = run_capture({"/usr/bin/xcode-select", "-p"});
    if (!selected || selected->exit_code != 0) return std::nullopt;
    std::string directory = selected->output;
    while (!directory.empty() && (directory.back() == '\n' || directory.back() == ' ')) directory.pop_back();
    return compiler_in(directory);
}

// The runtime every program links, built on this Mac by the compiler that will link it: an object from a newer
// compiler is not known to link with an older one, and no Mac but this one can tell. It is kept under a name
// made of what it was built from and by, so a new C Editor or new developer tools build their own.
std::expected<std::string, std::string> runtime_object(const CompileRequest& request, const std::string& compiler) {
    const std::string source = request.compat_dir + "/windows_runtime.c";
    const std::optional<FileStamp> from = file_stamp(source);
    const std::optional<FileStamp> by = file_stamp(compiler);
    if (!from || !by) return std::unexpected(source + " bulunamadı.\n");
    constexpr std::string_view prefix = "windows_runtime-";
    const std::string name = std::string(prefix) + std::string(architecture) + "-" + std::to_string(from->size) + "-" +
                             std::to_string(from->seconds) + "-" + std::to_string(by->size) + "-" + std::to_string(by->seconds) + ".o";
    const std::string object = request.cache_dir + "/" + name;
    if (::access(object.c_str(), R_OK) == 0) return object;

    ::mkdir(request.cache_dir.c_str(), 0700);
    // Under a name of this document's own first: another document may be building the same object right now.
    const std::string unfinished = object + "." + std::string(file_name(request.work_dir));
    const std::optional<Captured> built = run_capture(
        {"/usr/bin/clang", "-arch", std::string(architecture), "-std=gnu11", "-O2", "-fno-builtin", "-D_FORTIFY_SOURCE=0", "-w",
         "-fno-color-diagnostics", "-c", source, "-o", unfinished});
    if (!built || built->exit_code != 0 || ::rename(unfinished.c_str(), object.c_str()) != 0) {
        ::unlink(unfinished.c_str());
        return std::unexpected((built ? built->output : std::string()) + "C Editor’ün programlara eklediği kitaplık derlenemedi.\n");
    }
    if (DIR* entries = ::opendir(request.cache_dir.c_str())) {  // what earlier versions and compilers left
        while (const dirent* entry = ::readdir(entries)) {
            const std::string_view other = entry->d_name;
            // Not what another document is building under this name right now.
            if (other.starts_with(prefix) && !other.starts_with(name)) ::unlink((request.cache_dir + "/" + std::string(other)).c_str());
        }
        ::closedir(entries);
    }
    return object;
}

// Clang shows bytes that are not UTF-8 as "<FE>". Puts the letters back in the line it quotes under a message and
// takes the same width out of the marker lines under that, so the caret stays under its letter. The quote may
// or may not start with a line number: compilers before clang 17 show the bare line.
std::string readable_quote(std::string_view output) {
    const auto hex = [](char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); };
    const auto value = [](char c) { return c <= '9' ? c - '0' : c - 'A' + 10; };
    const auto is_mark = [&](std::string_view s, std::size_t at) {
        return at + 3 < s.size() && s[at] == '<' && hex(s[at + 1]) && hex(s[at + 2]) && s[at + 3] == '>' && s[at + 1] >= '8';
    };
    // What the line after this one is: the line a message quotes, the caret line under that, the line with the text
    // the compiler would insert (which only comes with a fix-it), or anything else.
    enum class Next { other, quote, caret, insertion } next = Next::other;
    std::string out;
    std::string_view rest = output;
    std::vector<std::size_t> marks;  // where "<XX>" began in the quoted line
    while (!rest.empty()) {
        const std::size_t newline = rest.find('\n');
        const std::string_view line = rest.substr(0, newline);
        rest.remove_prefix(newline == std::string_view::npos ? rest.size() : newline + 1);
        const bool message = is_message(line);
        const bool markers = (next == Next::caret && line.find('^') != line.npos && line.find_first_not_of(" ^~|") == line.npos) ||
                             (next == Next::insertion && rest.starts_with("fix-it:"));
        if (message) {
            out += utf8::valid(line) ? std::string(line) : decode(line, Encoding::windows1254);
            next = Next::quote;
        } else if (next == Next::quote && !line.starts_with("fix-it:")) {
            marks.clear();
            std::string bytes;
            for (std::size_t i = 0; i < line.size();) {
                if (is_mark(line, i)) {
                    marks.push_back(i);
                    bytes.push_back(static_cast<char>(value(line[i + 1]) * 16 + value(line[i + 2])));
                    i += 4;
                } else {
                    bytes.push_back(line[i++]);
                }
            }
            out += decode(bytes, marks.empty() ? Encoding::utf8 : Encoding::windows1254);
            next = Next::caret;
        } else if (markers) {
            for (std::size_t i = 0; i < line.size();) {
                out.push_back(line[i]);
                i += std::find(marks.begin(), marks.end(), i) != marks.end() ? 4 : 1;
            }
            next = next == Next::caret ? Next::insertion : Next::other;
        } else {
            out += line;
            next = Next::other;
        }
        out.push_back('\n');
    }
    return out;
}

// The compiler's sure correction, in words: what the line should say. The writer makes the change.
std::string describe(const Fix& fix, std::string_view text) {
    const std::string_view line = line_of(text, fix.line);
    const std::string before(line.substr(std::min(fix.column, line.size()), fix.end_column - fix.column));
    if (before.empty()) return "“" + fix.text + "” yazılmalı.";
    if (fix.text.empty()) return "“" + before + "” silinmeli.";
    return "“" + before + "” yerine “" + fix.text + "” yazılmalı.";
}

// The compiler's output with a Turkish explanation under each message it recognises, without the
// fix-it lines that only the editor reads.
std::string annotated(const std::string_view output, const CompileRequest& request,
                      const std::vector<Diagnostic>& diagnostics) {
    std::string log;
    std::string_view rest = output;
    std::size_t next = 0;  // diagnostics[next] belongs to the next message line about the source
    while (!rest.empty()) {
        const std::size_t newline = rest.find('\n');
        const std::string_view line = rest.substr(0, newline);
        rest.remove_prefix(newline == std::string_view::npos ? rest.size() : newline + 1);
        if (line.starts_with("fix-it:")) continue;
        log.append(line).push_back('\n');
        if (next == diagnostics.size() || parse_diagnostics(line, file_name(request.source_path)).empty()) continue;
        const Diagnostic& diagnostic = diagnostics[next++];
        std::string hint = explain(diagnostic.message, line_of(request.text, diagnostic.line), diagnostic.column);
        if (diagnostic.fix) hint += (hint.empty() ? "" : " ") + describe(*diagnostic.fix, request.text);
        if (!hint.empty()) log.append(hint_prefix).append(hint).push_back('\n');
    }
    if (const std::string link = explain_link_error(output); !link.empty())
        log.append(hint_prefix).append(link).push_back('\n');
    return log;
}

}  // namespace

CompileResult compile(const CompileRequest& request) {
    CompileResult result;
    const auto started = std::chrono::steady_clock::now();
    const std::optional<std::string> compiler = installed_compiler();
    if (!compiler) {
        result.no_compiler = true;
        return result;
    }
    ::mkdir(request.work_dir.c_str(), 0700);
    const std::string_view name = file_name(request.source_path);
    const bool cpp = is_cpp(request.source_path);
    const std::string stem(name.substr(0, name.rfind('.')));
    const std::string work_source = request.work_dir + "/" + std::string(name);
    result.binary = request.work_dir + "/" + stem;

    // A Windows-1254 file is compiled from its own bytes, as on Windows: 'ş' is one char and strlen("Şifre") is 5.
    // The runtime turns those bytes into the console's UTF-8. A UTF-8 file is compiled as it is.
    const bool single_byte = request.encoding == Encoding::windows1254;
    const std::expected<std::string, std::size_t> stored = encode(request.text, request.encoding);
    const std::expected<std::string, std::string> runtime = runtime_object(request, *compiler);
    // The compiler reads a copy of the user's file under its own name and runs in that folder, so diagnostics
    // name the file briefly; a #line directive would silence warnings that compare lines.
    if (!stored || !runtime || !write_file_atomic(work_source, *stored)) {
        result.log = !stored    ? "Metinde " + std::string(display_name(request.encoding)) + " ile saklanamayan bir karakter var.\n"
                     : !runtime ? runtime.error()
                                : work_source + " yazılamadı.\n";
        return result;
    }
    // clang has no "--": a name like "-x.c" is kept from reading as an option, and shown as itself below.
    const std::string argument = (name.starts_with('-') ? "./" : "") + std::string(name);
    std::vector<std::string> argv = {
        cpp ? "/usr/bin/clang++" : "/usr/bin/clang",
        "-arch", std::string(architecture),
        cpp ? "-std=gnu++14" : "-std=gnu11",
        "-O0", "-Wall", "-Wno-deprecated-declarations", "-fno-color-diagnostics", "-fdiagnostics-parseable-fixits",
        // A // comment ending in a backslash, common in escape-sequence lessons; the editor shows the joined line as comment.
        "-Wno-comment",
        // A global `index` is an ordinary name on Windows, whichever headers are read.
        "-fno-builtin-index", "-fno-builtin-rindex",
        // What only a run can see, answered by the runtime instead of the compiler's own library: integer division by
        // zero stops the program on every processor, and an index past the end of an array is said under the output.
        "-fsanitize=integer-divide-by-zero,array-bounds", "-fno-sanitize-link-runtime",
        // A local never set holds a pattern that reads as nonsense, not a 0 that looks right.
        "-ftrivial-auto-var-init=pattern",
        // GCC on Windows only warns about these old habits, e.g. `main()` or a missing #include. An older compiler
        // does not know every one of these names, and need not: it only warns about them already.
        "-Wno-unknown-warning-option", "-Wno-error=implicit-int", "-Wno-error=implicit-function-declaration",
        "-Wno-error=int-conversion", "-Wno-error=incompatible-function-pointer-types", "-Wno-error=return-mismatch",
        "-iquote", request.source_path.substr(0, request.source_path.rfind('/')),  // #include "own.h" beside it
        "-isystem", request.compat_dir,
        "-include", request.compat_dir + "/cedit_compat.h",
        "-o", result.binary,
        *runtime,  // ahead of the program, so the runtime is ready before anything of the program's own runs
        "-x", cpp ? "c++" : "c",
    };
    // Turkish letters in strings and comments are bytes the compiler passes on; the runtime is told what they are.
    if (single_byte) argv.insert(argv.end(), {"-Wno-invalid-source-encoding", "-DCEDIT_WINDOWS_1254"});
    argv.insert(argv.end(), {argument, "-lm"});
    std::optional<Captured> captured = run_capture(argv, request.work_dir);
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
    if (!captured) {
        result.log = "Derleyici başlatılamadı: " + argv[0] + "\n";
        return result;
    }
    result.ok = captured->exit_code == 0;
    if (argument != name)
        for (std::size_t at = 0; (at = captured->output.find(argument, at)) != std::string::npos;) captured->output.erase(at, 2);
    const std::string output = single_byte ? readable_quote(captured->output) : captured->output;
    result.diagnostics = parse_diagnostics(output, name);
    if (single_byte) {  // back from the bytes the compiler read to the editor's columns
        const auto to_editor = [&](std::size_t line, std::size_t column) { return utf8_column(line_of(request.text, line), column); };
        for (Diagnostic& diagnostic : result.diagnostics) {
            diagnostic.column = to_editor(diagnostic.line, diagnostic.column);
            if (!diagnostic.fix) continue;
            diagnostic.fix->column = to_editor(diagnostic.fix->line, diagnostic.fix->column);
            diagnostic.fix->end_column = to_editor(diagnostic.fix->line, diagnostic.fix->end_column);
        }
    }
    std::size_t errors = 0;
    std::size_t warnings = 0;
    for (const Diagnostic& diagnostic : result.diagnostics) (diagnostic.error ? errors : warnings) += 1;
    if (!result.ok && errors == 0) errors = 1;  // e.g. a linker error with no source location

    result.log = annotated(output, request, result.diagnostics);
    if (!result.log.empty()) result.log.push_back('\n');
    result.log += std::string(result.ok ? "Derleme başarılı (" : "Derleme başarısız (") + duration_text(elapsed.count()) +
                  "): " + count(errors, "hata") + ", " + count(warnings, "uyarı") + ".\n";
    return result;
}

std::optional<std::string> compiler_in(const std::string& developer_dir) {
    for (const char* clang : {"/usr/bin/clang", "/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"})
        if (::access((developer_dir + clang).c_str(), X_OK) == 0) return developer_dir + clang;
    return std::nullopt;
}

void request_developer_tools() { (void)run_capture({"/usr/bin/xcode-select", "--install"}); }

}  // namespace cedit
