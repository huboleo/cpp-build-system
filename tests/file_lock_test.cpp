#include "file_lock.hpp"
#include "test_support.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <future>
#include <gtest/gtest.h>
#include <optional>
#include <sys/file.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace {

// flock locks belong to an open file description, so opening the file again competes for the
// lock just like another process would.
bool is_locked(const std::filesystem::path& path) {
    const int descriptor = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (descriptor == -1) {
        return false;
    }

    const bool locked = flock(descriptor, LOCK_EX | LOCK_NB) == -1 && errno == EWOULDBLOCK;
    close(descriptor);
    return locked;
}

} // namespace

TEST(FileLock, FreeLockIsTakenWithoutWaiting) {
    bb::test::TempDir dir;
    bool waited = false;

    auto lock = bb::FileLock::acquire(dir / "lock", [&] { waited = true; });
    ASSERT_TRUE(lock) << lock.error();
    EXPECT_FALSE(waited);
    EXPECT_TRUE(is_locked(dir / "lock"));
}

TEST(FileLock, WaitsForTheHolderAndSaysSo) {
    bb::test::TempDir dir;
    const auto path = dir / "lock";

    auto held = bb::FileLock::acquire(path);
    ASSERT_TRUE(held) << held.error();

    std::promise<void> waiting;
    auto waiting_reported = waiting.get_future();
    std::atomic<bool> acquired = false;
    std::thread other{[&] {
        auto lock = bb::FileLock::acquire(path, [&] { waiting.set_value(); });
        acquired = lock.has_value();
    }};

    const bool reported =
        waiting_reported.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    const bool acquired_while_held = acquired;

    held->release();
    other.join();

    EXPECT_TRUE(reported);
    EXPECT_FALSE(acquired_while_held);
    EXPECT_TRUE(acquired);
}

TEST(FileLock, ReleaseAndDestructionUnlock) {
    bb::test::TempDir dir;
    const auto path = dir / "lock";

    {
        auto lock = bb::FileLock::acquire(path);
        ASSERT_TRUE(lock) << lock.error();

        lock->release();
        EXPECT_FALSE(is_locked(path));

        lock->release(); // a second release does nothing
    }

    {
        auto lock = bb::FileLock::acquire(path);
        ASSERT_TRUE(lock) << lock.error();
    }
    EXPECT_FALSE(is_locked(path));
}

TEST(FileLock, MovedLockStaysHeld) {
    bb::test::TempDir dir;
    const auto path = dir / "lock";
    std::optional<bb::FileLock> moved;

    {
        auto lock = bb::FileLock::acquire(path);
        ASSERT_TRUE(lock) << lock.error();
        moved.emplace(std::move(*lock));
    } // the moved-from lock is destroyed here

    EXPECT_TRUE(is_locked(path));
    moved.reset();
    EXPECT_FALSE(is_locked(path));
}
