#include "initializer.hpp"
#include "process.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::println(stderr, "Invalid usage. Check supported commands here...");
        exit(1);
    }

    std::string command = argv[1];
    if (command == "init") {
        auto result = bb::init();

        if (!result) {
            std::println(stderr, "error: {}", result.error());
            return 1;
        }

        std::println("initialized");
    } else if (command == "build") {
        std::error_code error;
        std::filesystem::create_directories(".cache/bb", error);

        if (error) {
            std::println(stderr, "error: {}", error.message());
            return 1;
        }

        // Compile bb's runner together with this project's build.cpp.
        auto compiled = bb::run_process({
            "clang++",
            "-std=c++23",
            "-I/Users/hubert/Projects/cpp-build-system/include",

            "/Users/hubert/Projects/cpp-build-system/src/build_runner.cpp",
            "/Users/hubert/Projects/cpp-build-system/src/build.cpp",
            "/Users/hubert/Projects/cpp-build-system/src/process.cpp",

            "build.cpp",
            "-o",
            ".cache/bb/build-runner",
        });

        if (!compiled) {
            std::println(stderr, "error: {}", compiled.error());
            return 1;
        }

        if (*compiled != 0) {
            return *compiled;
        }

        // Start the executable we just compiled.
        auto executed = bb::run_process({".cache/bb/build-runner"});

        if (!executed) {
            std::println(stderr, "error: {}", executed.error());
            return 1;
        }

        return *executed;
    } else if (command == "run") {
        std::println("running");
    }

    return 0;
}
