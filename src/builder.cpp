#include "builder.hpp"
#include "process.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

std::expected<std::filesystem::path, std::string> bb::build_project() {
    namespace fs = std::filesystem;
    const fs::path report_path{".cache/bb/executable-path"};
    std::error_code error;

    fs::create_directories(".cache/bb", error);
    if (error) {
        return std::unexpected("Cannot create runner directory: " + error.message());
    }

    // A failed build must never leave us reading a previous build's report.
    fs::remove(report_path, error);
    if (error) {
        return std::unexpected("Cannot remove old executable path report: " + error.message());
    }

    auto compiled = run_process({
        "clang++",
        "-std=c++23",
        "-I/Users/hubert/Projects/cpp-build-system/include",
        "/Users/hubert/Projects/cpp-build-system/src/build_runner.cpp",
        "/Users/hubert/Projects/cpp-build-system/src/build.cpp",
        "/Users/hubert/Projects/cpp-build-system/src/process.cpp",
        "build.cpp",
        BB_COMPILATION_DATABASE_LIBRARY,
        "-o",
        ".cache/bb/build-runner",
    });

    if (!compiled) {
        return std::unexpected(compiled.error());
    }

    if (*compiled != 0) {
        return std::unexpected("Runner compilation failed with exit code " +
                               std::to_string(*compiled));
    }

    auto executed = run_process({".cache/bb/build-runner"});
    if (!executed) {
        return std::unexpected(executed.error());
    }

    if (*executed != 0) {
        return std::unexpected("Project build failed with exit code " +
                               std::to_string(*executed));
    }

    std::ifstream report{report_path, std::ios::binary};
    if (!report) {
        return std::unexpected("Cannot open executable path report");
    }

    // Read the whole file: paths can contain spaces and even newlines.
    const std::string path{
        std::istreambuf_iterator<char>{report},
        std::istreambuf_iterator<char>{}
    };

    if (report.bad()) {
        return std::unexpected("Cannot read executable path report");
    }

    if (path.empty()) {
        return std::unexpected("Executable path report is empty");
    }

    return fs::path{path};
}
