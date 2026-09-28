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

enum class DatabaseWriteMode { CREATE, REPLACE };

// The entry for compiling source with flags, run from project_dir.
[[nodiscard]] CompileCommand source_command(const std::filesystem::path& project_dir,
                                            const std::vector<std::string>& flags,
                                            const std::string& source);

// The build configuration needs bb's SDK include directory in the editor too.
[[nodiscard]] CompileCommand build_configuration_command(const std::filesystem::path& project_dir);

// create refuses to overwrite; replace skips writing when the contents are unchanged.
[[nodiscard]] std::expected<void, std::string> write_compilation_database(
    const std::filesystem::path& path,
    std::span<const CompileCommand> commands,
    DatabaseWriteMode mode = DatabaseWriteMode::CREATE);

} // namespace bb
