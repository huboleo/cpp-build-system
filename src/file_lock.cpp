#include "file_lock.hpp"

#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>
#include <utility>

bb::FileLock::FileLock(int descriptor) : descriptor_{descriptor} {}

bb::FileLock::FileLock(FileLock&& other) noexcept
    : descriptor_{std::exchange(other.descriptor_, -1)} {}

bb::FileLock& bb::FileLock::operator=(FileLock&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (descriptor_ != -1) {
        close(descriptor_);
    }
    descriptor_ = std::exchange(other.descriptor_, -1);
    return *this;
}

bb::FileLock::~FileLock() {
    if (descriptor_ != -1) {
        close(descriptor_);
    }
}

std::expected<bb::FileLock, std::string>
bb::FileLock::acquire(const std::filesystem::path& path) {
    const int descriptor = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0666);
    if (descriptor == -1) {
        return std::unexpected("Cannot open build lock " + path.string() + ": " +
                               std::error_code(errno, std::generic_category()).message());
    }

    while (flock(descriptor, LOCK_EX) == -1) {
        if (errno == EINTR) {
            continue;
        }

        const auto error = std::error_code(errno, std::generic_category()).message();
        close(descriptor);
        return std::unexpected("Cannot acquire build lock " + path.string() + ": " + error);
    }

    return FileLock{descriptor};
}
