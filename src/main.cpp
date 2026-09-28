#include "builder.hpp"
#include "initializer.hpp"
#include <cstdio>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view USAGE = R"(usage: bb <command> [options]

commands:
  init                Create a project in the current directory
  build               Build the project
  run [-- <args>...]  Build the project, then run its executable with <args>
  help                Show this message
)";

int usage_error(const std::string& message) {
    std::println(stderr, "error: {}\n", message);
    std::print(stderr, "{}", USAGE);
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> arguments(argv + 1, argv + argc);

    if (arguments.empty()) {
        std::print(stderr, "{}", USAGE);
        return 1;
    }

    const auto& command = arguments.front();
    const auto options = std::span{arguments}.subspan(1);

    if (command == "help" || command == "--help" || command == "-h") {
        std::print("{}", USAGE);
        return 0;
    }

    if (command == "init") {
        if (!options.empty()) {
            return usage_error("unexpected argument '" + options[0] + "' for init");
        }

        auto result = bb::init();

        if (!result) {
            std::println(stderr, "error: {}", result.error());
            return 1;
        }

        std::println("initialized");
        return 0;
    }

    if (command == "build" || command == "run") {
        const auto build_command =
            command == "build"
                ? bb::BuildCommand::BUILD
                : bb::BuildCommand::RUN;

        std::vector<std::string> run_arguments;
        if (build_command == bb::BuildCommand::RUN && !options.empty() && options[0] == "--") {
            run_arguments.assign(options.begin() + 1, options.end());
        } else if (!options.empty()) {
            const std::string hint =
                build_command == bb::BuildCommand::RUN ? "; pass program arguments after --" : "";
            return usage_error("unexpected argument '" + options[0] + "' for " + command + hint);
        }

        // On success this process becomes the build runner, so reaching the next line means
        // the build could not start.
        const auto error = bb::build_project(build_command, std::move(run_arguments));
        std::println(stderr, "error: {}", error);
        return 1;
    }

    return usage_error("unknown command '" + command + "'");
}
