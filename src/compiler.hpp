#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace bb {

struct CompilerIdentity {
    // The path to invoke. Symlinks are kept, because clang picks its driver mode from the name
    // it is invoked by: calling clang++ through its target (e.g. clang-23) compiles as C.
    std::filesystem::path path;
    // The binary that path points to, used to notice when the compiler changes.
    std::filesystem::path resolved_path;
    std::string version;
    std::uintmax_t size;
    std::int64_t modification_time;
};

[[nodiscard]]
std::expected<CompilerIdentity, std::string>
identify_compiler(std::string_view executable);

} // namespace bb
