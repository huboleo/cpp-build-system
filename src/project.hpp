#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace bb {

// Returns the nearest directory, starting at start and walking up, that contains build.cpp.
[[nodiscard]] std::expected<std::filesystem::path, std::string>
find_project_root(const std::filesystem::path& start);

} // namespace bb
