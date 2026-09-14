#pragma once

#include "compiler.hpp"

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
    CompilerIdentity compiler;
    std::vector<std::string> command;
    std::map<std::string, std::string> environment;
    std::map<std::string, std::string> files; // path -> content hash, sorted by path
};

struct FingerprintRecord {
    std::string fingerprint;
    std::string runner_hash;
    std::map<std::string, std::string> files;
};

// Returns a BLAKE3 hash over a canonical encoding of inputs. Every field is
// length-prefixed, so no two distinct inputs can encode to the same bytes.
[[nodiscard]]
std::string compute_fingerprint(const FingerprintInputs& inputs);

// Writes the input fingerprint and runner digest to path, replacing any existing file.
[[nodiscard]]
std::expected<void, std::string>
write_fingerprint_record(const std::filesystem::path& path,
                         const FingerprintInputs& inputs,
                         std::string_view runner_hash);

// Returns the cache record stored at path, or nullopt when there is no usable record.
// Missing, unreadable, malformed and obsolete files all mean "no cache", not an error.
[[nodiscard]]
std::optional<FingerprintRecord>
read_fingerprint_record(const std::filesystem::path& path);

} // namespace bb
