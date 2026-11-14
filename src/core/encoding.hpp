#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace cedit {

enum class Encoding : std::uint8_t { utf8, windows1254 };

// How a file is stored on disk. The editor always works on UTF-8 text with '\n' line ends.
struct FileFormat {
    Encoding encoding = Encoding::utf8;
    bool byte_order_mark = false;
    bool crlf = false;
    friend bool operator==(const FileFormat&, const FileFormat&) = default;
};

// How Turkish Windows saves text files; new files use it, so they open unchanged on Windows PCs.
constexpr FileFormat windows_turkish{.encoding = Encoding::windows1254, .crlf = true};

struct LoadedText {
    std::string text;
    FileFormat format;
    bool mixed_line_ends = false;  // both CRLF and LF: storing makes them all `format.crlf`
};

// Valid UTF-8 with a BOM or with non-ASCII text loads as UTF-8; anything else is Windows-1254, the code
// page of Turkish Windows, in which every byte has a character and survives a round trip. Plain ASCII
// reads the same in both, so it loads as Windows-1254 too.
LoadedText load_text(std::string_view bytes);

// Error value: byte offset in `text` of the first character the encoding cannot represent.
std::expected<std::string, std::size_t> store_text(std::string_view text, const FileFormat& format);

std::string decode(std::string_view bytes, Encoding encoding);
std::expected<std::string, std::size_t> encode(std::string_view text, Encoding encoding);

// Column `column` of a line stored in Windows-1254, one byte per character, as a byte column of the same
// line in UTF-8 `line`. Columns past the end stay that far past it.
std::size_t utf8_column(std::string_view line, std::size_t column);

constexpr std::string_view display_name(Encoding encoding) {
    return encoding == Encoding::utf8 ? "UTF-8" : "Windows-1254";
}

}  // namespace cedit
