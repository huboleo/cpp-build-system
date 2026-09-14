#include "builder.hpp"
#include "fingerprint.hpp"
#include "hashing.hpp"
#include "process.hpp"

#include <cstdio>
#include <expected>
#include <filesystem>
#include <map>
#include <print>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

// build.cpp plus everything bb contributes to the runner: the SDK headers it may
// include, and the static library it links against.
std::expected<std::map<std::string, std::string>, std::string> hash_runner_inputs() {
    std::map<std::string, std::string> files;

    const auto add = [&files](const fs::path& path) -> std::expected<void, std::string> {
        auto hash = bb::hash_file(path);
        if (!hash) {
            return std::unexpected(hash.error());
        }

        files.emplace(path.string(), std::move(*hash));
        return {};
    };

    if (auto result = add("build.cpp"); !result) {
        return std::unexpected(result.error());
    }

    if (auto result = add(BB_RUNTIME_LIBRARY); !result) {
        return std::unexpected(result.error());
    }

    std::error_code error;
    fs::recursive_directory_iterator entry{BB_SDK_INCLUDE_DIR, error};
    if (error) {
        return std::unexpected("Cannot read SDK include directory: " + error.message());
    }

    const fs::recursive_directory_iterator end;
    for (; entry != end; entry.increment(error)) {
        if (error) {
            return std::unexpected("Cannot read SDK include directory: " + error.message());
        }

        const bool regular = entry->is_regular_file(error);
        if (error) {
            return std::unexpected("Cannot inspect " + entry->path().string() + ": " +
                                   error.message());
        }

        if (!regular) {
            continue;
        }

        if (auto result = add(entry->path()); !result) {
            return std::unexpected(result.error());
        }
    }

    return files;
}

} // namespace

std::expected<int, std::string>
bb::build_project(BuildCommand command) {
    std::error_code error;

    fs::create_directories(".cache/bb", error);
    if (error) {
        return std::unexpected("Cannot create runner directory: " + error.message());
    }

    const fs::path runner_path{".cache/bb/build-runner"};
    const fs::path record_path{".cache/bb/build-runner.json"};

    const std::vector<std::string> compile_command{
        "clang++",
        "-std=c++23",
        "-I",
        BB_SDK_INCLUDE_DIR,
        "build.cpp",
        BB_RUNTIME_LIBRARY,
        "-o",
        runner_path.string(),
    };

    auto files = hash_runner_inputs();
    if (!files) {
        return std::unexpected(files.error());
    }

    const FingerprintInputs inputs{
        .command = compile_command,
        .files = std::move(*files),
    };

    const bool runner_exists = fs::exists(runner_path, error) && !error;
    const auto recorded = read_fingerprint_record(record_path);
    const bool up_to_date =
        runner_exists && recorded && *recorded == compute_fingerprint(inputs);

    if (up_to_date) {
        std::println(stderr, "build runner: cached");
    } else {
        std::println(stderr, "build runner: out of date, compiling");

        // A failed compile must not leave a record claiming the old binary is current.
        fs::remove(record_path, error);
        if (error) {
            return std::unexpected("Cannot remove stale fingerprint record: " + error.message());
        }

        auto compiled = run_process(compile_command);
        if (!compiled) {
            return std::unexpected(compiled.error());
        }

        if (*compiled != 0) {
            return std::unexpected("Runner compilation failed with exit code " +
                                   std::to_string(*compiled));
        }

        if (auto written = write_fingerprint_record(record_path, inputs); !written) {
            return std::unexpected(written.error());
        }
    }

    const auto runner_command = command == BuildCommand::BUILD ? "build" : "run";

    return run_process({runner_path.string(), runner_command});
}
