#include "builder.hpp"
#include "build_runner_cache.hpp"
#include "file_lock.hpp"
#include "process.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

std::expected<int, std::string>
bb::build_project(BuildCommand command) {
    std::error_code error;

    fs::create_directories(".cache/bb", error);
    if (error) {
        return std::unexpected("Cannot create runner directory: " + error.message());
    }

    auto build_lock = FileLock::acquire(".cache/bb/build-runner.lock");
    if (!build_lock) {
        return std::unexpected(build_lock.error());
    }

    auto runner = prepare_build_runner();
    if (!runner) {
        return std::unexpected(runner.error());
    }

    const auto runner_command = command == BuildCommand::BUILD ? "build" : "run";

    return run_process({runner->string(), runner_command});
}
