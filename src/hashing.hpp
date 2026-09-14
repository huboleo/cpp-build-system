#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace bb {

// Returns the file's BLAKE3 hash as a 64-character hexadecimal string.
// Returns an error if the file cannot be opened or read.
[[nodiscard]]
std::expected<std::string, std::string>
hash_file(const std::filesystem::path& path);

// Returns data's BLAKE3 hash as a 64-character hexadecimal string.
[[nodiscard]]
std::string hash_bytes(std::string_view data);

} // namespace bb
