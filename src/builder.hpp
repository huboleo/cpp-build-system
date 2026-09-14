#pragma once

#include <expected>
#include <string>

namespace bb {

enum class BuildCommand {
    BUILD,
    RUN,
};

// Prepares and launches the build runner for the requested command.
// Returns the runner's exit code, or an error preparing/launching it.
[[nodiscard]]
std::expected<int, std::string> build_project(BuildCommand command);

} // namespace bb
