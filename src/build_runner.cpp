#include "bb/build.hpp"
#include <filesystem>
#include <print>
#include <system_error>

int main(int argc, char** argv) {
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
}
