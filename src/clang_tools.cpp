#include "clang_tools.hpp"
#include "executable_search.hpp"
#include "process.hpp"
#include "project.hpp"
#include "string_utils.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr std::array<std::string_view, 13> SOURCE_EXTENSIONS{
    ".c", ".cc", ".c++", ".cpp", ".cxx", ".cppm", ".ixx",
    ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp",
};

// Homebrew installs LLVM without adding it to PATH.
constexpr std::array<std::string_view, 3> HOMEBREW_LLVM_DIRECTORIES{
    "/opt/homebrew/opt/llvm/bin",
    "/usr/local/opt/llvm/bin",
    "/home/linuxbrew/.linuxbrew/opt/llvm/bin",
};

// Keeps each command line well below the operating system's length limit.
constexpr std::size_t FILES_PER_INVOCATION = 100;

bool is_source_file(const fs::path& path) {
    return std::ranges::find(SOURCE_EXTENSIONS, path.extension().string()) !=
           SOURCE_EXTENSIONS.end();
}

// Moves into the project directory, so tools run with project-relative paths like a build does.
std::expected<fs::path, std::string> enter_project() {
    std::error_code error;
    const auto current_dir = fs::current_path(error);
    if (error) {
        return std::unexpected("Cannot determine current directory: " + error.message());
    }

    auto project_dir = bb::find_project_root(current_dir);
    if (!project_dir) {
        return std::unexpected(project_dir.error());
    }

    fs::current_path(*project_dir, error);
    if (error) {
        return std::unexpected("Cannot enter project directory " + project_dir->string() + ": " +
                               error.message());
    }

    return project_dir;
}

#ifdef __APPLE__
// compile_commands.json names the compiler as plain clang++, so a clang-tidy from another
// toolchain (e.g. Homebrew's) finds neither the SDK nor the standard library headers. clangd
// works around this by asking xcrun, and clang's macOS driver reads the SDK from SDKROOT.
void point_to_macos_sdk() {
    if (std::getenv("SDKROOT") != nullptr) {
        return;
    }

    auto sdk = bb::run_process_capture({"xcrun", "--show-sdk-path"});
    if (sdk && sdk->exit_code == 0) {
        const auto path = string_utils::trim(sdk->output);
        if (!path.empty()) {
            setenv("SDKROOT", path.c_str(), 0);
        }
    }
}
#endif

// Runs the command once per batch of files and returns the highest exit code.
std::expected<int, std::string> run_on_files(const std::vector<std::string>& command,
                                             const std::vector<fs::path>& files) {
    int worst = 0;

    for (std::size_t begin = 0; begin < files.size(); begin += FILES_PER_INVOCATION) {
        auto arguments = command;
        const auto end = std::min(files.size(), begin + FILES_PER_INVOCATION);
        for (auto index = begin; index < end; ++index) {
            arguments.push_back(files[index].string());
        }

        auto result = bb::run_process(std::move(arguments));
        if (!result) {
            return std::unexpected(result.error());
        }
        worst = std::max(worst, *result);
    }

    return worst;
}

} // namespace

std::expected<fs::path, std::string> bb::find_clang_tool(std::string_view name) {
    if (auto found = find_executable(name)) {
        return found;
    }

#ifdef __APPLE__
    if (auto xcrun = run_process_capture({"xcrun", "--find", std::string{name}});
        xcrun && xcrun->exit_code == 0) {
        if (auto found = find_executable(string_utils::trim(xcrun->output))) {
            return found;
        }
    }
#endif

    for (const auto directory : HOMEBREW_LLVM_DIRECTORIES) {
        if (auto found = find_executable((fs::path{directory} / name).string())) {
            return found;
        }
    }

    return std::unexpected("Cannot find " + std::string{name} +
                           "; install LLVM (for example with brew install llvm, or your "
                           "distribution's clang-tools package) or add it to PATH");
}

std::expected<std::vector<fs::path>, std::string>
bb::formattable_files(const fs::path& project_dir) {
    std::vector<fs::path> files;
    std::error_code error;
    fs::recursive_directory_iterator entry{project_dir,
                                           fs::directory_options::skip_permission_denied, error};
    const fs::recursive_directory_iterator end;

    for (; !error && entry != end; entry.increment(error)) {
        const auto name = entry->path().filename().string();

        // Entries that can't be inspected, such as broken symlinks, are skipped.
        std::error_code entry_error;
        if (entry->is_directory(entry_error)) {
            const bool output_directory = entry.depth() == 0 && name == "bb";
            if (output_directory || name.starts_with('.')) {
                entry.disable_recursion_pending();
            }
            continue;
        }

        if (entry->is_regular_file(entry_error) && is_source_file(entry->path())) {
            files.push_back(entry->path().lexically_relative(project_dir));
        }
    }

    if (error) {
        return std::unexpected("Cannot list files in " + project_dir.string() + ": " +
                               error.message());
    }

    std::ranges::sort(files);
    return files;
}

std::expected<std::vector<fs::path>, std::string> bb::lintable_files(const fs::path& project_dir) {
    const auto database_path = project_dir / "compile_commands.json";
    std::ifstream input{database_path, std::ios::binary};
    if (!input) {
        return std::unexpected("Cannot open " + database_path.string() +
                               "; run bb build to create it");
    }

    const auto database = nlohmann::json::parse(input, nullptr, false);
    if (!database.is_array()) {
        return std::unexpected("Invalid compilation database " + database_path.string());
    }

    std::vector<fs::path> files;
    for (const auto& entry : database) {
        if (!entry.is_object() || !entry.contains("directory") || !entry.contains("file") ||
            !entry["directory"].is_string() || !entry["file"].is_string()) {
            return std::unexpected("Invalid entry in " + database_path.string());
        }

        const auto file =
            (fs::path{entry["directory"].get<std::string>()} / entry["file"].get<std::string>())
                .lexically_normal();
        files.push_back(file.lexically_relative(project_dir));
    }

    std::ranges::sort(files);
    const auto duplicates = std::ranges::unique(files);
    files.erase(duplicates.begin(), duplicates.end());
    return files;
}

std::expected<int, std::string> bb::format_project(bool check) {
    auto project_dir = enter_project();
    if (!project_dir) {
        return std::unexpected(project_dir.error());
    }

    auto tool = find_clang_tool("clang-format");
    if (!tool) {
        return std::unexpected(tool.error());
    }

    auto files = formattable_files(*project_dir);
    if (!files) {
        return std::unexpected(files.error());
    }

    std::vector<std::string> command{tool->string(), "--style=file"};
    if (check) {
        command.insert(command.end(), {"--dry-run", "-Werror"});
    } else {
        command.emplace_back("-i");
    }

    return run_on_files(command, *files);
}

std::expected<int, std::string> bb::lint_project() {
    auto project_dir = enter_project();
    if (!project_dir) {
        return std::unexpected(project_dir.error());
    }

    auto tool = find_clang_tool("clang-tidy");
    if (!tool) {
        return std::unexpected(tool.error());
    }

    auto files = lintable_files(*project_dir);
    if (!files) {
        return std::unexpected(files.error());
    }

#ifdef __APPLE__
    point_to_macos_sdk();
#endif

    return run_on_files({tool->string(), "-p", project_dir->string(), "--quiet"}, *files);
}
