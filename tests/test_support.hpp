#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace bb::test {

// A fresh directory in the system temp directory, removed with its contents at the end of the
// scope. The path is canonical, because bb compares it with fs::current_path().
class TempDir {
  public:
    TempDir() {
        auto pattern = (std::filesystem::temp_directory_path() / "bb-test-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr) {
            throw std::runtime_error("cannot create a temporary directory");
        }
        _path = std::filesystem::canonical(pattern);
    }

    ~TempDir() {
        std::error_code ignored;
        std::filesystem::remove_all(_path, ignored);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return _path; }

    [[nodiscard]] std::filesystem::path operator/(const std::filesystem::path& relative) const {
        return _path / relative;
    }

  private:
    std::filesystem::path _path;
};

// Makes path the working directory until the end of the scope.
class ScopedCurrentPath {
  public:
    explicit ScopedCurrentPath(const std::filesystem::path& path)
        : _previous{std::filesystem::current_path()} {
        std::filesystem::current_path(path);
    }

    ~ScopedCurrentPath() {
        std::error_code ignored;
        std::filesystem::current_path(_previous, ignored);
    }

    ScopedCurrentPath(const ScopedCurrentPath&) = delete;
    ScopedCurrentPath& operator=(const ScopedCurrentPath&) = delete;

  private:
    std::filesystem::path _previous;
};

// Sets an environment variable until the end of the scope.
class ScopedEnvironment {
  public:
    ScopedEnvironment(std::string name, const std::string& value) : _name{std::move(name)} {
        if (const char* previous = std::getenv(_name.c_str())) {
            _previous = previous;
        }
        setenv(_name.c_str(), value.c_str(), 1);
    }

    ~ScopedEnvironment() {
        if (_previous) {
            setenv(_name.c_str(), _previous->c_str(), 1);
        } else {
            unsetenv(_name.c_str());
        }
    }

    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

  private:
    std::string _name;
    std::optional<std::string> _previous;
};

inline void write_file(const std::filesystem::path& path, std::string_view contents) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output << contents;
    if (!output) {
        throw std::runtime_error("cannot write " + path.string());
    }
}

// Writes a shell script that can be run like a program.
inline void write_executable(const std::filesystem::path& path, std::string_view script) {
    write_file(path, script);
    std::filesystem::permissions(path, std::filesystem::perms::owner_all);
}

inline std::string read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("cannot read " + path.string());
    }

    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

// Files replaced through a temporary file and rename get a new inode, so comparing inodes tells
// whether a file was rewritten, regardless of timestamp resolution.
inline ino_t inode_of(const std::filesystem::path& path) {
    struct stat info {};
    if (stat(path.c_str(), &info) != 0) {
        throw std::runtime_error("cannot stat " + path.string());
    }
    return info.st_ino;
}

} // namespace bb::test
