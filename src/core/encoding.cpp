#include "encoding.hpp"

#include <algorithm>
#include <array>

#include "utf8.hpp"

namespace cedit {
namespace {

// Windows-1254 bytes 0x80-0xFF. The seven the code page leaves undefined are the C1 controls of the
// same number, as Windows itself maps them, so that every byte survives a round trip.
constexpr std::array<char16_t, 128> windows1254_high = [] {
    std::array<char16_t, 128> table{};
    constexpr char16_t bytes_80_9f[32] = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,  // 80-87
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x008E, 0x008F,  // 88-8F
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,  // 90-97
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x009E, 0x0178,  // 98-9F
    };
    for (std::size_t i = 0; i < 32; ++i) table[i] = bytes_80_9f[i];
    for (std::size_t i = 32; i < 128; ++i) table[i] = static_cast<char16_t>(0x80 + i);  // Latin-1 range
    table[0xD0 - 0x80] = 0x011E;  // Ğ
    table[0xDD - 0x80] = 0x0130;  // İ
    table[0xDE - 0x80] = 0x015E;  // Ş
    table[0xF0 - 0x80] = 0x011F;  // ğ
    table[0xFD - 0x80] = 0x0131;  // ı
    table[0xFE - 0x80] = 0x015F;  // ş
    return table;
}();

constexpr std::string_view bom = "\xEF\xBB\xBF";

}  // namespace

std::size_t utf8_column(std::string_view line, std::size_t column) {
    std::size_t offset = 0;
    for (; column > 0 && offset < line.size(); --column) offset = utf8::next(line, offset);
    return offset + column;
}

std::string decode(std::string_view bytes, Encoding encoding) {
    if (encoding == Encoding::utf8) return std::string(bytes);
    std::string out;
    out.reserve(bytes.size() + bytes.size() / 8);
    for (const char byte : bytes) {
        const auto value = static_cast<unsigned char>(byte);
        if (value < 0x80) {
            out.push_back(byte);
            continue;
        }
        utf8::append(out, windows1254_high[value - 0x80u]);
    }
    return out;
}

std::expected<std::string, std::size_t> encode(std::string_view text, Encoding encoding) {
    if (encoding == Encoding::utf8) return std::string(text);
    std::string out;
    out.reserve(text.size());
    for (std::size_t offset = 0; offset < text.size();) {
        const utf8::Decoded decoded = utf8::decode(text, offset);
        if (decoded.code_point < 0x80) {
            out.push_back(static_cast<char>(decoded.code_point));
        } else {
            std::size_t index = 0;
            while (index < windows1254_high.size() && char32_t{windows1254_high[index]} != decoded.code_point) ++index;
            if (index == windows1254_high.size()) return std::unexpected(offset);
            out.push_back(static_cast<char>(0x80 + index));
        }
        offset += decoded.length;
    }
    return out;
}

LoadedText load_text(std::string_view bytes) {
    LoadedText loaded;
    const std::string_view body = bytes.starts_with(bom) ? bytes.substr(bom.size()) : bytes;
    const bool ascii = std::ranges::all_of(body, [](char c) { return static_cast<unsigned char>(c) < 0x80u; });
    if (utf8::valid(body) && (body.size() != bytes.size() || !ascii)) {
        loaded.format.encoding = Encoding::utf8;
        loaded.format.byte_order_mark = body.size() != bytes.size();
        bytes = body;
    } else {
        loaded.format.encoding = Encoding::windows1254;
    }
    const std::string decoded = decode(bytes, loaded.format.encoding);
    std::size_t crlf = 0;
    std::size_t lf = 0;
    loaded.text.reserve(decoded.size());
    for (std::size_t i = 0; i < decoded.size(); ++i) {
        if (decoded[i] == '\n') ++(i > 0 && decoded[i - 1] == '\r' ? crlf : lf);
        if (decoded[i] == '\r' && i + 1 < decoded.size() && decoded[i + 1] == '\n') continue;
        loaded.text.push_back(decoded[i]);
    }
    loaded.format.crlf = crlf > 0 && crlf >= lf;
    loaded.mixed_line_ends = crlf > 0 && lf > 0;
    return loaded;
}

std::expected<std::string, std::size_t> store_text(std::string_view text, const FileFormat& format) {
    std::expected<std::string, std::size_t> encoded = encode(text, format.encoding);
    if (!encoded) return encoded;
    std::string out;
    out.reserve(encoded->size() + (format.crlf ? encoded->size() / 16 : 0) + bom.size());
    if (format.byte_order_mark && format.encoding == Encoding::utf8) out.append(bom);
    for (const char byte : *encoded) {
        if (byte == '\n' && format.crlf) out.push_back('\r');
        out.push_back(byte);
    }
    return out;
}

}  // namespace cedit
