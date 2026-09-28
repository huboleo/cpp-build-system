#include "compiler.hpp"
#include "executable_search.hpp"
#include "process.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

std::expected<bb::CompilerIdentity, std::string>
bb::identify_compiler(std::string_view executable) {
    auto path = find_executable(executable);
    if (!path) {
        return std::unexpected("Cannot use compiler: " + path.error());
    }

    std::error_code error;
    auto resolved_path = fs::canonical(*path, error);
    if (error) {
        return std::unexpected("Cannot resolve compiler " + path->string() + ": " +
                               error.message());
    }

    const auto size = fs::file_size(resolved_path, error);
    if (error) {
        return std::unexpected("Cannot read compiler size: " + error.message());
    }

    const auto modification_time = fs::last_write_time(resolved_path, error);
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
        .resolved_path = std::move(resolved_path),
        .version = std::move(version->output),
        .size = size,
        .modification_time =
            static_cast<std::int64_t>(modification_time.time_since_epoch().count()),
    };
}
