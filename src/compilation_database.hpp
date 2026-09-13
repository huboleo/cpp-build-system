#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace bb {

struct CompileCommand {
    std::filesystem::path directory;
    std::filesystem::path file;
    std::vector<std::string> arguments;
};

enum class DatabaseWriteMode { CREATE, MERGE, REPLACE };

// The build configuration needs bb's SDK include directory in the editor too.
[[nodiscard]] CompileCommand build_configuration_command(const std::filesystem::path& project_dir);

// create refuses to overwrite; merge preserves entries for other source files.
[[nodiscard]] std::expected<void, std::string> write_compilation_database(
    const std::filesystem::path& path,
    std::span<const CompileCommand> commands,
    DatabaseWriteMode mode = DatabaseWriteMode::CREATE);

} // namespace bb
