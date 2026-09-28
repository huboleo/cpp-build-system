#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <string>

namespace bb {

class FileLock {
  public:
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    FileLock(FileLock&& other) noexcept;
    FileLock& operator=(FileLock&& other) noexcept;

    ~FileLock();

    // Releases the lock before destruction. Does nothing if it was already released.
    void release();

    // Blocks until the lock is free. Calls on_wait first if another process holds it.
    [[nodiscard]]
    static std::expected<FileLock, std::string>
    acquire(const std::filesystem::path& path, const std::function<void()>& on_wait = {});

  private:
    explicit FileLock(int descriptor);

    int descriptor_;
};

} // namespace bb
