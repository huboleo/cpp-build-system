#include "builder.hpp"
#include "process.hpp"

#include <filesystem>
#include <system_error>

std::expected<int, std::string>
bb::build_project(BuildCommand command) {
    namespace fs = std::filesystem;
    std::error_code error;

    fs::create_directories(".cache/bb", error);
    if (error) {
        return std::unexpected("Cannot create runner directory: " + error.message());
    }

    auto compiled = run_process({
        "clang++",
        "-std=c++23",
        "-I",
        BB_SDK_INCLUDE_DIR,
        "build.cpp",
        BB_RUNTIME_LIBRARY,
        "-o",
        ".cache/bb/build-runner",
    });

    if (!compiled) {
        return std::unexpected(compiled.error());
    }

    if (*compiled != 0) {
        return std::unexpected("Runner compilation failed with exit code " +
                               std::to_string(*compiled));
    }

    const auto runner_command =
        command == BuildCommand::BUILD ? "build" : "run";

    return run_process({
        ".cache/bb/build-runner",
        runner_command,
    });
}
