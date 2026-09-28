#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace bb {

// Returns an absolute path to the executable without resolving symlinks. A name without a
// directory is looked up in PATH.
[[nodiscard]]
std::expected<std::filesystem::path, std::string> find_executable(std::string_view executable);

} // namespace bb
