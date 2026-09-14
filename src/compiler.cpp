#include "compiler.hpp"
#include "process.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace fs = std::filesystem;

namespace {

bool is_executable_file(const fs::path& path) {
    std::error_code error;
    return fs::is_regular_file(path, error) && !error && access(path.c_str(), X_OK) == 0;
}

std::expected<fs::path, std::string> resolve_executable(std::string_view executable) {
    const fs::path requested{executable};

    if (requested.has_parent_path()) {
        if (!is_executable_file(requested)) {
            return std::unexpected("Compiler is not an executable file: " + requested.string());
        }

        std::error_code error;
        auto resolved = fs::canonical(requested, error);
        if (error) {
            return std::unexpected("Cannot resolve compiler " + requested.string() + ": " +
                                   error.message());
        }
        return resolved;
    }

    const char* path_environment = std::getenv("PATH");
    if (path_environment == nullptr) {
        return std::unexpected("Cannot find compiler because PATH is not set");
    }

    const std::string_view search_path{path_environment};
    std::size_t begin = 0;
    while (begin <= search_path.size()) {
        const auto end = search_path.find(':', begin);
        const auto directory = search_path.substr(begin, end - begin);
        const fs::path candidate = directory.empty() ? requested : fs::path{directory} / requested;

        if (is_executable_file(candidate)) {
            std::error_code error;
            auto resolved = fs::canonical(candidate, error);
            if (error) {
                return std::unexpected("Cannot resolve compiler " + candidate.string() + ": " +
                                       error.message());
            }
            return resolved;
        }

        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }

    return std::unexpected("Cannot find compiler in PATH: " + requested.string());
}

} // namespace

std::expected<bb::CompilerIdentity, std::string>
bb::identify_compiler(std::string_view executable) {
    auto path = resolve_executable(executable);
    if (!path) {
        return std::unexpected(path.error());
    }

    std::error_code error;
    const auto size = fs::file_size(*path, error);
    if (error) {
        return std::unexpected("Cannot read compiler size: " + error.message());
    }

    const auto modification_time = fs::last_write_time(*path, error);
    if (error) {
        return std::unexpected("Cannot read compiler modification time: " + error.message());
    }

    auto version = run_process_capture({path->string(), "--version"});
    if (!version) {
        return std::unexpected(version.error());
    }
    if (version->exit_code != 0) {
        return std::unexpected("Compiler version command failed with exit code " +
                               std::to_string(version->exit_code));
    }
    if (version->output.empty()) {
        return std::unexpected("Compiler version command produced no output");
    }

    return CompilerIdentity{
        .path = std::move(*path),
        .version = std::move(version->output),
        .size = size,
        .modification_time =
            static_cast<std::int64_t>(modification_time.time_since_epoch().count()),
    };
}
