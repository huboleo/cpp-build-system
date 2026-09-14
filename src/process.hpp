#pragma once

#include <expected>
#include <string>
#include <vector>

namespace bb {

struct ProcessOutput {
    int exit_code;
    std::string output;
};

// Inherits the working directory, environment, and terminal streams, then waits.
// Returns the exit code, 128 + signal for signal termination, or 127 if exec fails.
// Invalid arguments and fork/wait failures are returned as errors.
[[nodiscard]] std::expected<int, std::string> run_process(std::vector<std::string> arguments);

// Captures the process's standard output and standard error into one string.
[[nodiscard]]
std::expected<ProcessOutput, std::string>
run_process_capture(std::vector<std::string> arguments);

} // namespace bb
