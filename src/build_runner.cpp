#include "bb/build.hpp"
#include "process.hpp"
#include <filesystem>
#include <print>
#include <system_error>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
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

    std::vector<std::string> arguments{
        "clang++",
        "-std=c++23",
    };

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
    return 0;
}
