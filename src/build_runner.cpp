#include "bb/build.hpp"
#include "compilation_database.hpp"
#include "compile_flags.hpp"
#include "file_lock.hpp"
#include "process.hpp"
#include <algorithm>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Deletes anything in the build directory that this build did not produce.
void remove_stale_outputs(const std::vector<std::string>& current) {
    std::error_code error;
    fs::directory_iterator entry{"bb/build/debug/bin", error};
    if (error) {
        return;
    }

    const fs::directory_iterator end;
    for (; entry != end; entry.increment(error)) {
        if (error) {
            return;
        }

        if (std::ranges::find(current, entry->path().string()) != current.end()) {
            continue;
        }

        fs::remove_all(entry->path(), error);
        if (error) {
            std::println(stderr, "warning: cannot remove stale output '{}': {}",
                         entry->path().string(), error.message());
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::println(stderr, "usage: build-runner <build|run> [<program-arguments>...]");
        return 1;
    }

    const std::string_view command{argv[1]};

    if (command != "build" && command != "run") {
        std::println(stderr, "error: unknown command '{}'", command);
        return 1;
    }

    bb::Build b;
    build(b);

    for (const auto& error : b.errors()) {
        std::println(stderr, "{}:{}:{}: error: {}", error.location.file_name(),
                     error.location.line(), error.location.column(), error.message);
    }

    if (!b.errors().empty()) {
        return 1;
    }

    // The runner compiles one executable in a single compiler invocation. Libraries and
    // multiple executables need separate compile and link steps first.
    if (!b.libraries().empty()) {
        std::println(stderr, "error: libraries are not supported by the build runner yet");
        return 1;
    }

    if (b.executables().empty()) {
        std::println(stderr, "error: no executable declared; call b.executable(...)");
        return 1;
    }

    if (b.executables().size() > 1) {
        std::println(stderr, "error: the build runner supports only one executable for now");
        return 1;
    }

    const auto& target = b.executables().front();

    for (const auto& source : target.sources) {
        std::error_code error;
        const bool is_file = std::filesystem::is_regular_file(source, error);

        if (error) {
            std::println(stderr, "error: cannot access '{}': {}", source, error.message());
            return 1;
        }

        if (!is_file) {
            std::println(stderr, "error: source '{}' is missing or is not a file", source);
            return 1;
        }
    }

    // Held while writing build outputs, and released before running the program, so builds
    // in other terminals don't wait for it to exit.
    auto build_lock = bb::FileLock::acquire("bb/cache/build.lock", [] {
        std::println(stderr, "waiting for another bb process in this project to finish");
    });
    if (!build_lock) {
        std::println(stderr, "error: {}", build_lock.error());
        return 1;
    }

    std::error_code error;
    fs::create_directories("bb/build/debug/bin", error);

    if (error) {
        std::println(stderr, "error: cannot create build directory: {}", error.message());
        return 1;
    }

    const auto output_path = fs::path{"bb/build/debug/bin"} / target.name;

    // Used by both compile_commands.json and the final compiler invocation.
    const auto flags = bb::target_flags(b.cpp_standard(), target.include_paths);

    const auto project_dir = fs::current_path(error);
    if (error) {
        std::println(stderr, "error: cannot determine project directory: {}", error.message());
        return 1;
    }

    // Construct records for compile_commands.json
    std::vector<bb::CompileCommand> commands{bb::build_configuration_command(project_dir)};
    for (const auto& source : target.sources) {
        commands.push_back(bb::source_command(project_dir, flags, source));
    }

    auto database = bb::write_compilation_database(project_dir / "compile_commands.json", commands,
                                                   bb::DatabaseWriteMode::REPLACE);
    if (!database) {
        std::println(stderr, "error: {}", database.error());
        return 1;
    }

    // Construct final command for compiler invocation
    std::vector<std::string> arguments{"clang++"};
    arguments.insert(arguments.end(), flags.begin(), flags.end());
    for (const auto& source : target.sources) {
        arguments.push_back(source);
    }
    arguments.emplace_back("-o");
    arguments.push_back(output_path.string());

    auto result = bb::run_process(std::move(arguments));

    if (!result) {
        std::println(stderr, "error: {}", result.error());
        return 1;
    }

    if (*result != 0) {
        return *result;
    }

    remove_stale_outputs({output_path.string()});
    build_lock->release();

    if (command == "build") {
        return 0;
    }

    const auto executable = fs::absolute(output_path, error);

    if (error) {
        std::println(stderr, "error: cannot resolve executable path: {}", error.message());
        return 1;
    }

    std::vector<std::string> program{executable.string()};
    program.insert(program.end(), argv + 2, argv + argc);

    const auto exec_error = bb::replace_process(std::move(program));
    std::println(stderr, "error: {}", exec_error);
    return 1;
}
