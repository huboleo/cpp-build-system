#pragma once

#include <string>
#include <vector>

namespace bb {

enum class BuildCommand {
    BUILD,
    RUN,
};

// Prepares the build runner for the project containing the current directory, then replaces
// this process with it. For RUN, the runner passes run_arguments to the executable it runs.
// Returns only if preparing or launching the runner fails, with the error message.
[[nodiscard]]
std::string build_project(BuildCommand command, std::vector<std::string> run_arguments = {});

} // namespace bb
