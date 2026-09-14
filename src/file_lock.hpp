#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace bb {

class FileLock {
  public:
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    FileLock(FileLock&& other) noexcept;
    FileLock& operator=(FileLock&& other) noexcept;

    ~FileLock();

    [[nodiscard]]
    static std::expected<FileLock, std::string> acquire(const std::filesystem::path& path);

  private:
    explicit FileLock(int descriptor);

    int descriptor_;
};

} // namespace bb
