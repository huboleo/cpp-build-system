#include "bb/build.hpp"
#include "compilation_database.hpp"
#include "process.hpp"
#include <cstdio>
#include <filesystem>
#include <print>
#include <string_view>
#include <system_error>
#include <utility>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;

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

    const std::vector<std::string> compiler_arguments{
        "clang++",
        "-std=c++23",
    };

    const auto project_dir = fs::current_path(error);
    if (error) {
        std::println(stderr, "error: cannot determine project directory: {}", error.message());
        return 1;
    }

    std::vector<bb::CompileCommand> commands{
        bb::build_configuration_command(project_dir)
    };
    for (const auto& source : target.sources) {
        auto source_arguments = compiler_arguments;
        source_arguments.push_back("-c");
        source_arguments.push_back(source);
        commands.push_back({project_dir, source, std::move(source_arguments)});
    }

    auto database = bb::write_compilation_database(
        project_dir / "compile_commands.json", commands, bb::DatabaseWriteMode::REPLACE);
    if (!database) {
        std::println(stderr, "error: {}", database.error());
        return 1;
    }

    auto arguments = compiler_arguments;
    for (const auto& source : target.sources) {
        arguments.push_back(source);
    }

    arguments.push_back("-o");
    arguments.push_back(output_path.string());

    std::println("Building {}", target.name);

    auto result = bb::run_process(std::move(arguments));

    if (!result) {
        std::println(stderr, "error: {}", result.error());
        return 1;
    }

    if (*result != 0) {
        return *result;
    }

    std::println("Built {}", output_path.string());

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
