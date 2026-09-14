#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace bb {

// Reads the dependencies from a Make-style file emitted by Clang's -MD option.
[[nodiscard]]
std::expected<std::vector<std::filesystem::path>, std::string>
read_dependency_file(const std::filesystem::path& path);

} // namespace bb
