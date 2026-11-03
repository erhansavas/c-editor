#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace cedit {

std::optional<std::string> read_file(const std::string& path);

// Tells whether a file changed since it was last seen. Empty if there is no such file.
struct FileStamp {
    long long size, seconds, nanoseconds;
    unsigned long long inode;
    friend bool operator==(const FileStamp&, const FileStamp&) = default;
};
std::optional<FileStamp> file_stamp(const std::string& path);

// Writes a sibling temporary file and renames it over `path`, so a failed save never truncates the
// original. Refuses a read-only file; a symbolic link stays a link to the file that is written.
bool write_file_atomic(const std::string& path, std::string_view bytes);

}  // namespace cedit
