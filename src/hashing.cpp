#include "hashing.hpp"

#include <array>
#include <blake3.h>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

std::string to_hex(const std::array<std::uint8_t, BLAKE3_OUT_LEN>& digest) {
    std::string result;
    result.reserve(digest.size() * 2);

    for (auto byte : digest) {
        std::format_to(std::back_inserter(result), "{:02x}", byte);
    }

    return result;
}

std::string finalize(blake3_hasher& hasher) {
    std::array<std::uint8_t, BLAKE3_OUT_LEN> digest;
    blake3_hasher_finalize(&hasher, digest.data(), digest.size());
    return to_hex(digest);
}

} // namespace

std::expected<std::string, std::string> bb::hash_file(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        return std::unexpected("Cannot open file for hashing: " + path.string());
    }

    std::array<char, 64 * 1024> buffer;
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    // read() sets failbit on the final short read, so the stream tests false while
    // gcount() still reports real bytes. Checking gcount() keeps that last chunk; the
    // following iteration then exits, because read() on a failed stream zeroes gcount().
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        blake3_hasher_update(&hasher, buffer.data(), static_cast<std::size_t>(file.gcount()));
    }

    if (file.bad() || !file.eof()) {
        return std::unexpected("Cannot read file for hashing: " + path.string());
    }

    return finalize(hasher);
}

std::string bb::hash_bytes(std::string_view data) {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.data(), data.size());
    return finalize(hasher);
}
