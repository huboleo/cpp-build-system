#include "build_runner_cache.hpp"

#include "compiler.hpp"
#include "dependency_file.hpp"
#include "fingerprint.hpp"
#include "hashing.hpp"
#include "process.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <map>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::expected<std::map<std::string, std::string>, std::string>
hash_files(const std::vector<fs::path>& paths) {
    std::map<std::string, std::string> files;

    for (const auto& path : paths) {
        auto hash = bb::hash_file(path);
        if (!hash) {
            return std::unexpected(hash.error());
        }

        files.emplace(path.string(), std::move(*hash));
    }

    return files;
}

std::map<std::string, std::string> compilation_environment() {
    constexpr std::array<std::string_view, 6> names{
        "CPATH",
        "CPLUS_INCLUDE_PATH",
        "DEVELOPER_DIR",
        "LIBRARY_PATH",
        "MACOSX_DEPLOYMENT_TARGET",
        "SDKROOT",
    };

    std::map<std::string, std::string> environment;
    for (const auto name : names) {
        if (const char* value = std::getenv(name.data())) {
            environment.emplace(name, value);
        }
    }

    return environment;
}

} // namespace

std::expected<fs::path, std::string> bb::prepare_build_runner() {
    const fs::path runner_path{".cache/bb/build-runner"};
    const fs::path temporary_runner_path{".cache/bb/build-runner.tmp"};
    const fs::path record_path{".cache/bb/build-runner.json"};
    const fs::path dependency_path{".cache/bb/build-runner.d"};
    const fs::path temporary_dependency_path{".cache/bb/build-runner.d.tmp"};

    auto compiler = identify_compiler("clang++");
    if (!compiler) {
        return std::unexpected(compiler.error());
    }

    const std::vector<std::string> compile_command{
        compiler->path.string(),
        "-std=c++23",
        "-I",
        BB_SDK_INCLUDE_DIR,
        "-MMD",
        "-MF",
        temporary_dependency_path.string(),
        "-MT",
        "build-runner",
        "build.cpp",
        BB_RUNTIME_LIBRARY,
        "-o",
        temporary_runner_path.string(),
    };
    const auto environment = compilation_environment();

    std::error_code error;
    const bool runner_is_regular = fs::is_regular_file(runner_path, error) && !error;
    const bool runner_is_executable =
        runner_is_regular && access(runner_path.c_str(), X_OK) == 0;
    const auto recorded = read_fingerprint_record(record_path);
    bool up_to_date = false;

    if (runner_is_executable && recorded) {
        auto runner_hash = hash_file(runner_path);
        if (runner_hash && *runner_hash == recorded->runner_hash) {
            std::vector<fs::path> recorded_paths;
            recorded_paths.reserve(recorded->files.size());
            for (const auto& file : recorded->files) {
                recorded_paths.emplace_back(file.first);
            }

            if (auto files = hash_files(recorded_paths)) {
                const FingerprintInputs inputs{
                    .compiler = *compiler,
                    .command = compile_command,
                    .environment = environment,
                    .files = std::move(*files),
                };
                up_to_date = recorded->fingerprint == compute_fingerprint(inputs);
            }
        }
    }

    if (up_to_date) {
        std::println(stderr, "build runner: cached");
        return runner_path;
    }

    std::println(stderr, "build runner: out of date, compiling");

    // A failed compile must not leave a record claiming the old binary is current.
    fs::remove(record_path, error);
    if (error) {
        return std::unexpected("Cannot remove stale fingerprint record: " + error.message());
    }

    fs::remove(dependency_path, error);
    if (error) {
        return std::unexpected("Cannot remove stale dependency file: " + error.message());
    }

    fs::remove(temporary_runner_path, error);
    if (error) {
        return std::unexpected("Cannot remove temporary runner: " + error.message());
    }

    fs::remove(temporary_dependency_path, error);
    if (error) {
        return std::unexpected("Cannot remove temporary dependency file: " + error.message());
    }

    auto compiled = run_process(compile_command);
    if (!compiled) {
        return std::unexpected(compiled.error());
    }

    if (*compiled != 0) {
        return std::unexpected("Runner compilation failed with exit code " +
                               std::to_string(*compiled));
    }

    auto dependencies = read_dependency_file(temporary_dependency_path);
    if (!dependencies) {
        return std::unexpected(dependencies.error());
    }
    dependencies->emplace_back(BB_RUNTIME_LIBRARY);

    auto files = hash_files(*dependencies);
    if (!files) {
        return std::unexpected(files.error());
    }

    const FingerprintInputs inputs{
        .compiler = *compiler,
        .command = compile_command,
        .environment = environment,
        .files = std::move(*files),
    };

    auto runner_hash = hash_file(temporary_runner_path);
    if (!runner_hash) {
        return std::unexpected(runner_hash.error());
    }

    fs::rename(temporary_dependency_path, dependency_path, error);
    if (error) {
        return std::unexpected("Cannot publish dependency file: " + error.message());
    }

    fs::rename(temporary_runner_path, runner_path, error);
    if (error) {
        return std::unexpected("Cannot publish build runner: " + error.message());
    }

    if (auto written = write_fingerprint_record(record_path, inputs, *runner_hash); !written) {
        return std::unexpected(written.error());
    }

    return runner_path;
}
