#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace bb {

// Builds the project in the current working directory and returns its executable.
[[nodiscard]] std::expected<std::filesystem::path, std::string> build_project();

} // namespace bb
