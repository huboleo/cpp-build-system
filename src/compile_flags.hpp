#pragma once

#include "bb/build.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace bb {

// Compiler flags shared by the real compiles and their compile_commands.json entries, so clangd
// sees exactly what gets compiled.

// Flags for compiling build.cpp into the build runner. bb's headers come in through -isystem,
// so warnings like -pedantic apply to the user's build.cpp only.
[[nodiscard]] std::vector<std::string> build_file_flags();

// Every file in bb's SDK include directory. -MMD leaves -isystem headers out of dependency
// files, so the build runner's fingerprint has to list them itself.
[[nodiscard]] std::expected<std::vector<std::filesystem::path>, std::string> sdk_headers();

// Flags for compiling a target's sources.
[[nodiscard]] std::vector<std::string> target_flags(CppStandard standard,
                                                    const std::vector<std::string>& include_paths);

} // namespace bb
