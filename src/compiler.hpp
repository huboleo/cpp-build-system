#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace bb {

struct CompilerIdentity {
    std::filesystem::path path;
    std::string version;
    std::uintmax_t size;
    std::int64_t modification_time;
};

[[nodiscard]]
std::expected<CompilerIdentity, std::string>
identify_compiler(std::string_view executable);

} // namespace bb
