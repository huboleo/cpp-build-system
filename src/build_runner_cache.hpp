#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace bb {

// Returns the cached runner path, compiling and publishing a new runner when needed.
[[nodiscard]]
std::expected<std::filesystem::path, std::string> prepare_build_runner();

} // namespace bb
