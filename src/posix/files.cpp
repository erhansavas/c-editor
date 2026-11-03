#include "files.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>

namespace cedit {
namespace {

bool write_all(int fd, std::string_view bytes) {
    while (!bytes.empty()) {
        const ssize_t written = ::write(fd, bytes.data(), bytes.size());
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        bytes.remove_prefix(static_cast<std::size_t>(written));
    }
    return true;
}

}  // namespace

std::optional<std::string> read_file(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return std::nullopt;
    std::string bytes;
    char buffer[1 << 16];
    for (;;) {
        const ssize_t got = ::read(fd, buffer, sizeof buffer);
        if (got < 0 && errno == EINTR) continue;
        if (got < 0) {
            ::close(fd);
            return std::nullopt;
        }
        if (got == 0) break;
        bytes.append(buffer, static_cast<std::size_t>(got));
    }
    ::close(fd);
    return bytes;
}

std::optional<FileStamp> file_stamp(const std::string& path) {
    struct stat info {};
    if (::stat(path.c_str(), &info) != 0) return std::nullopt;
    return FileStamp{info.st_size, info.st_mtimespec.tv_sec, info.st_mtimespec.tv_nsec, info.st_ino};
}

bool write_file_atomic(const std::string& path, std::string_view bytes) {
    // Through a link to the file it names: the rename below would replace the link instead.
    char resolved[PATH_MAX];
    const std::string target = ::realpath(path.c_str(), resolved) ? resolved : path;
    mode_t mode = 0644;
    struct stat existing {};
    if (::stat(target.c_str(), &existing) == 0) {
        if (::access(target.c_str(), W_OK) != 0) return false;  // the rename would overwrite a read-only file
        mode = existing.st_mode & 07777;
    }
    std::string temporary = target + ".cedit-XXXXXX";
    const int fd = ::mkstemp(temporary.data());
    if (fd < 0) return false;
    const bool ok = write_all(fd, bytes) && ::fchmod(fd, mode) == 0 && ::fsync(fd) == 0;
    ::close(fd);
    if (!ok || std::rename(temporary.c_str(), target.c_str()) != 0) {
        ::unlink(temporary.c_str());
        return false;
    }
    return true;
}

}  // namespace cedit
