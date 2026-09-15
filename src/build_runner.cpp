#include "bb/build.hpp"
#include "compilation_database.hpp"
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
    fs::directory_iterator entry{"build", error};
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

std::string_view cpp_standard_enum_to_string(bb::CppStandard standard) {
    switch (standard) {
    case bb::CppStandard::CPP_11:
        return "-std=c++11";
    case bb::CppStandard::CPP_14:
        return "-std=c++14";
    case bb::CppStandard::CPP_17:
        return "-std=c++17";
    case bb::CppStandard::CPP_20:
        return "-std=c++20";
    case bb::CppStandard::CPP_23:
        return "-std=c++23";
    }
    return "-std=c++23";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::println(stderr, "usage: build-runner <build|run>");
        return 1;
    }

    const std::string_view command{argv[1]};

    if (command != "build" && command != "run") {
        std::println(stderr, "error: unknown command '{}'", command);
        return 1;
    }

    bb::Build b;
    build(b);

    if (auto error = b.error()) {
        switch (*error) {
        case bb::BuildError::EXECUTABLE_ALREADY_DECLARED:
            std::println(stderr, "error: only one executable can be declared");
            break;
        }

        return 1;
    }

    if (!b.target()) {
        std::println("error: no executable declared; call b.executable(...)");
        return 1;
    }

    const auto& target = *b.target();

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
    std::error_code error;
    fs::create_directories("build", error);

    if (error) {
        std::println(stderr, "error: cannot create build directory: {}", error.message());
        return 1;
    }

    const auto output_path = fs::path{"build"} / target.name;

    // Prepare starting commands for both compile_commands.json and final executable compiler
    // invocation
    std::vector<std::string> compiler_arguments{
        "clang++",
        std::string{cpp_standard_enum_to_string(b.cpp_standard())},
    };
    for (const auto& include : target.include_paths) {
        compiler_arguments.emplace_back("-I");
        compiler_arguments.push_back(include);
    }

    const auto project_dir = fs::current_path(error);
    if (error) {
        std::println(stderr, "error: cannot determine project directory: {}", error.message());
        return 1;
    }

    // Construct records for compile_commands.json
    std::vector<bb::CompileCommand> commands{bb::build_configuration_command(project_dir)};
    for (const auto& source : target.sources) {
        auto source_arguments = compiler_arguments;
        source_arguments.emplace_back("-c");
        source_arguments.push_back(source);
        commands.push_back({project_dir, source, std::move(source_arguments)});
    }

    auto database = bb::write_compilation_database(project_dir / "compile_commands.json", commands,
                                                   bb::DatabaseWriteMode::REPLACE);
    if (!database) {
        std::println(stderr, "error: {}", database.error());
        return 1;
    }

    // Construct final command for compiler invocation
    auto arguments = compiler_arguments;
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

    if (command == "build") {
        return 0;
    }

    const auto executable = fs::absolute(output_path, error);

    if (error) {
        std::println(stderr, "error: cannot resolve executable path: {}", error.message());
        return 1;
    }

    auto executed = bb::run_process({executable.string()});
    if (!executed) {
        std::println(stderr, "error: {}", executed.error());
        return 1;
    }

    return *executed;
}
