#include "builder.hpp"
#include "initializer.hpp"
#include "process.hpp"
#include <cstdio>
#include <cstdlib>
#include <print>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::println(stderr, "Invalid usage. Check supported commands here...");
        exit(1);
    }

    std::string command = argv[1];
    if (command == "init") {
        auto mode = bb::InitMode::new_project;
        if (argc == 3 && std::string{argv[2]} == "--existing") {
            mode = bb::InitMode::existing_project;
        } else if (argc != 2) {
            std::println(stderr, "Usage: bb init [--existing]");
            return 1;
        }

        auto result = bb::init(mode);

        if (!result) {
            std::println(stderr, "error: {}", result.error());
            return 1;
        }

        if (mode == bb::InitMode::existing_project) {
            std::println("initialized; edit build.cpp to list your executable's sources before building");
        } else {
            std::println("initialized");
        }
    } else if (command == "build" || command == "run") {
        auto result = bb::build_project();

        if (!result) {
            std::println(stderr, "error: {}", result.error());
            return 1;
        }

        if (command == "run") {
            auto executed = bb::run_process({result->string()});

            if (!executed) {
                std::println(stderr, "error: {}", executed.error());
                return 1;
            }

            return *executed;
        }
    }

    return 0;
}
