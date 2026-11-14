// What the test files share: CHECK, compiling the way ⌘R does, and a program on a console.
#pragma once

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/encoding.hpp"
#include "core/terminal.hpp"
#include "core/utf8.hpp"
#include "posix/compile.hpp"
#include "posix/files.hpp"
#include "posix/process.hpp"

inline int failures = 0;
inline std::string compat_dir;      // the folder programs are compiled against: a build's own, or an installed app's
inline std::string scratch_dir;     // build/tmp/tests: files there are overwritten on every run
inline std::string local_programs;  // tests/local, if the person building has one

inline void check(bool condition, const char* what, int line) {
    if (condition) return;
    ++failures;
    std::fprintf(stderr, "FAIL line %d: %s\n", line, what);
}
#define CHECK(condition) check((condition), #condition, __LINE__)

inline bool has(std::string_view text, std::string_view wanted) { return text.find(wanted) != std::string_view::npos; }

inline std::string utf8_of(const std::u32string& line) {
    std::string out;
    for (const char32_t c : line) cedit::utf8::append(out, c);
    return out;
}

// Compiles the file at `source` as the editor would after loading it.
inline cedit::CompileResult compile_file(const std::string& source, const std::string& work_dir) {
    const cedit::LoadedText loaded = cedit::load_text(cedit::read_file(source).value_or(""));
    return cedit::compile({source, loaded.text, loaded.format.encoding, compat_dir, work_dir, scratch_dir + "/cache"});
}

// Saves `bytes` as `name` and compiles it; the log is printed when the build does not end as expected.
inline cedit::CompileResult build(const std::string& name, const std::string& bytes, bool should_build = true) {
    const std::string source = scratch_dir + "/" + name;
    CHECK(cedit::write_file_atomic(source, bytes));
    cedit::CompileResult result = compile_file(source, scratch_dir + "/work");
    if (result.ok != should_build) std::fprintf(stderr, "%s", result.log.c_str());
    return result;
}

// A compiled program on a pty, seen through the console's Terminal; it answers each prompt only
// once it is on screen, as a person at the keyboard would.
struct Console {
    cedit::PtyChild child;
    cedit::Terminal screen;
    std::string shown;

    bool read_until(std::string_view wanted) {
        char buffer[4096];
        while (shown.find(wanted) == std::string::npos) {
            pollfd ready{child.master, POLLIN, 0};
            if (::poll(&ready, 1, 10'000) <= 0) return false;  // a broken program fails the test instead of hanging it
            const ssize_t got = ::read(child.master, buffer, sizeof buffer);
            if (got < 0 && errno == EINTR) continue;
            if (got <= 0) return false;  // EIO once the program has exited
            screen.feed(std::string_view(buffer, static_cast<std::size_t>(got)));
            shown.clear();
            for (const auto& line : screen.lines()) shown += utf8_of(line) + "\n";
        }
        return true;
    }

    void type(std::string_view keys) {
        CHECK(::write(child.master, keys.data(), keys.size()) == static_cast<ssize_t>(keys.size()));
    }

    int finish() {
        read_until("\x01");           // drain to the end
        ::kill(child.pid, SIGKILL);  // no effect on a program that has exited
        int status = 0;
        ::waitpid(child.pid, &status, 0);
        ::close(child.master);
        if (failures) std::fprintf(stderr, "console:\n%s", shown.c_str());
        return cedit::exit_code_of(status);
    }
};

inline std::optional<Console> start(const cedit::CompileResult& result) {
    if (!result.ok) return std::nullopt;
    const auto child = cedit::spawn_in_pty(result.binary, scratch_dir, cedit::Terminal::columns, 25);
    CHECK(child.has_value());
    if (!child) return std::nullopt;
    ::fcntl(child->master, F_SETFL, 0);  // blocking reads are simpler here
    return Console{*child, {}, {}};
}

// What a program shows and how it ends; `keys` are typed as soon as it runs.
inline std::pair<std::string, int> run(const std::string& name, const std::string& code, std::string_view keys = {}) {
    std::optional<Console> console = start(build(name, code));
    if (!console) return {"", -1};
    if (!keys.empty()) console->type(keys);
    const int exit_code = console->finish();
    return {console->shown, exit_code};
}

void core_tests();
void program_tests();
