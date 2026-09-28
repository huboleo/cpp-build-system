#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace bb {

// Finds a clang tool such as clang-format or clang-tidy: in PATH, then in Xcode (macOS ships
// clang-format there, outside PATH), then in Homebrew's LLVM, which isn't in PATH by default.
[[nodiscard]] std::expected<std::filesystem::path, std::string>
find_clang_tool(std::string_view name);

// The project's C and C++ files, relative to project_dir and sorted. Skips bb's output
// directory and hidden directories such as .git.
[[nodiscard]] std::expected<std::vector<std::filesystem::path>, std::string>
formattable_files(const std::filesystem::path& project_dir);

// The files listed in the project's compile_commands.json, relative to project_dir and sorted.
[[nodiscard]] std::expected<std::vector<std::filesystem::path>, std::string>
lintable_files(const std::filesystem::path& project_dir);

// Formats the project containing the current directory with clang-format. With check, files are
// left unchanged and the result is nonzero if any of them needs formatting.
// Returns clang-format's exit code.
[[nodiscard]] std::expected<int, std::string> format_project(bool check);

// Runs clang-tidy on the project containing the current directory, with the flags from
// compile_commands.json. Returns clang-tidy's exit code.
[[nodiscard]] std::expected<int, std::string> lint_project();

} // namespace bb
