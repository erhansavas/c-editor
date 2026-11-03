#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cedit {

// A correction clang is sure of, e.g. a missing ';': replace bytes [column, end_column) of `line` with `text`.
struct Fix {
    std::size_t line;  // 0-based
    std::size_t column;
    std::size_t end_column;
    std::string text;
};

struct Diagnostic {
    std::size_t line;    // 0-based
    std::size_t column;  // 0-based byte column as reported by clang
    bool error;          // error or fatal error; otherwise warning
    std::string message;
    std::optional<Fix> fix;  // when clang gives exactly one, on one line
};

// clang "path:line:col: error|warning|fatal error: message" lines that refer to `source_path`, with the
// "fix-it:" lines of -fdiagnostics-parseable-fixits that belong to them.
std::vector<Diagnostic> parse_diagnostics(std::string_view compiler_output, std::string_view source_path);

// Where a failed build sends the caret: its first error or, when only the linker failed, the call it
// could not resolve. Null if neither exists.
const Diagnostic* first_problem(const std::vector<Diagnostic>& diagnostics);

// A short Turkish explanation for a beginner of a common compiler message about `source_line`, or "".
std::string explain(std::string_view message, std::string_view source_line);

// The same for the linker's "Undefined symbols" error, which has no line of its own.
std::string explain_link_error(std::string_view compiler_output);

}  // namespace cedit
