#include "builder.hpp"
#include "build_runner_cache.hpp"
#include "file_lock.hpp"
#include "process.hpp"

#include <cstdio>
#include <expected>
#include <filesystem>
#include <iterator>
#include <print>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Returns the nearest directory, starting at start and walking up, that contains build.cpp.
std::expected<fs::path, std::string> find_project_root(const fs::path& start) {
    std::error_code error;

    for (auto directory = start;; directory = directory.parent_path()) {
        const auto build_file = directory / "build.cpp";
        if (fs::exists(build_file, error)) {
            return directory;
        }

        if (error) {
            return std::unexpected("Cannot inspect " + build_file.string() + ": " +
                                   error.message());
        }

        if (directory == directory.parent_path()) {
            return std::unexpected("Cannot find build.cpp in this directory or any parent; "
                                   "run bb init to create a project");
        }
    }
}

} // namespace

std::string bb::build_project(BuildCommand command, std::vector<std::string> run_arguments) {
    std::error_code error;

    const auto current_dir = fs::current_path(error);
    if (error) {
        return "Cannot determine current directory: " + error.message();
    }

    auto project_dir = find_project_root(current_dir);
    if (!project_dir) {
        return project_dir.error();
    }

    // The runner and everything below use paths relative to the project directory.
    fs::current_path(*project_dir, error);
    if (error) {
        return "Cannot enter project directory " + project_dir->string() + ": " +
               error.message();
    }

    fs::create_directories("bb/cache", error);
    if (error) {
        return "Cannot create runner directory: " + error.message();
    }

    auto runner_lock = FileLock::acquire("bb/cache/build-runner.lock", [] {
        std::println(stderr, "waiting for another bb process in this project to finish");
    });
    if (!runner_lock) {
        return runner_lock.error();
    }

    auto runner = prepare_build_runner();
    if (!runner) {
        return runner.error();
    }

    // This lock only covers checking and compiling the runner. The runner takes its own lock
    // for the build and releases it before running the program.
    runner_lock->release();

    std::vector<std::string> arguments{runner->string(),
                                       command == BuildCommand::BUILD ? "build" : "run"};
    arguments.insert(arguments.end(), std::make_move_iterator(run_arguments.begin()),
                     std::make_move_iterator(run_arguments.end()));

    // bb becomes the runner, and for run the runner becomes the program, so the program ends
    // up with the terminal and signals, and its exit code is what the shell sees.
    return replace_process(std::move(arguments));
}
