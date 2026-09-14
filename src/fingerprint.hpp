#pragma once

#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bb {

// Identifies the encoding used below. It is part of the hashed data, so changing the
// encoding automatically invalidates every fingerprint written by an older bb.
inline constexpr std::string_view FINGERPRINT_VERSION = "bb-fingerprint-v1";

struct FingerprintInputs {
    std::vector<std::string> command;
    std::map<std::string, std::string> files; // path -> content hash, sorted by path
};

// Returns a BLAKE3 hash over a canonical encoding of inputs. Every field is
// length-prefixed, so no two distinct inputs can encode to the same bytes.
[[nodiscard]]
std::string compute_fingerprint(const FingerprintInputs& inputs);

// Writes fingerprint, command and files to path as JSON, replacing any existing file.
[[nodiscard]]
std::expected<void, std::string>
write_fingerprint_record(const std::filesystem::path& path, const FingerprintInputs& inputs);

// Returns the fingerprint recorded at path, or nullopt when there is no usable record.
// Missing, unreadable and malformed files all mean "no cache", not an error.
[[nodiscard]]
std::optional<std::string>
read_fingerprint_record(const std::filesystem::path& path);

} // namespace bb
