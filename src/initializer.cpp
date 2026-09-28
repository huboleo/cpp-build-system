#include "initializer.hpp"
#include "bb/build.hpp"
#include "compilation_database.hpp"
#include "compile_flags.hpp"
#include "process.hpp"
#include "string_utils.hpp"
#include <array>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

void remove_created_path(const std::filesystem::path& path, std::string& diagnostic) {
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error) {
        diagnostic += "\nCleanup failed for " + path.string() + ": " + error.message();
    }
}

std::expected<void, std::string> finish_file(std::ofstream& output,
                                             const std::filesystem::path& path) {
    output.close();
    if (!output) {
        std::string diagnostic = "Could not write " + path.string();
        remove_created_path(path, diagnostic);
        return std::unexpected(diagnostic);
    }

    return {};
}

std::expected<void, std::string> write_main_file(const std::filesystem::path& main_file) {
    std::ofstream main_output{main_file, std::ios::out | std::ios::noreplace};

    if (!main_output) {
        return std::unexpected("Could not open " + main_file.string());
    }

    main_output << R"(#include <print>

int main() {
    std::println("Hello!");
    return 0;
}
)";

    return finish_file(main_output, main_file);
}

std::expected<void, std::string> write_build_file(const std::filesystem::path& build_file) {
    std::ofstream build_output{build_file, std::ios::out | std::ios::noreplace};

    if (!build_output) {
        return std::unexpected("Could not open " + build_file.string());
    }

    build_output << R"(#include <bb/build.hpp>

void build(bb::Build& b) {
    b.executable({.name = "app", .sources = {"src/main.cpp"}});
}
)";

    return finish_file(build_output, build_file);
}

// The generated sources above are already formatted with this style.
constexpr std::string_view CLANG_FORMAT = R"(BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
DerivePointerAlignment: false
PointerAlignment: Left
AllowShortFunctionsOnASingleLine: Empty
AllowShortLambdasOnASingleLine: Empty
AllowShortBlocksOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
AllowShortCaseLabelsOnASingleLine: false
)";

// Broad check families without the noisiest checks. Some of the exclusions would otherwise fire
// on the generated project itself: exception-escape on main() calling std::println, and
// identifier-length on build.cpp's `b` parameter.
constexpr std::string_view CLANG_TIDY = R"(Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  -bugprone-exception-escape,
  clang-analyzer-*,
  cppcoreguidelines-*,
  -cppcoreguidelines-avoid-magic-numbers,
  misc-*,
  -misc-non-private-member-variables-in-classes,
  modernize-*,
  -modernize-use-trailing-return-type,
  performance-*,
  portability-*,
  readability-*,
  -readability-identifier-length,
  -readability-magic-numbers
WarningsAsErrors: ''
)";

// Writes a configuration file unless the project already has one (e.g. a cloned repository),
// in which case it's kept. Returns whether the file was created.
std::expected<bool, std::string> write_config_file(const std::filesystem::path& path,
                                                   std::string_view contents) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        return std::unexpected("Could not inspect " + path.string() + ": " + error.message());
    }

    if (exists) {
        return false;
    }

    std::ofstream output{path, std::ios::binary | std::ios::noreplace};
    if (!output) {
        return std::unexpected("Could not open " + path.string());
    }

    output << contents;
    if (auto result = finish_file(output, path); !result) {
        return std::unexpected(result.error());
    }

    return true;
}

// Anchored with a leading slash, so they don't also match nested directories like include/bb.
constexpr std::array<std::string_view, 2> GITIGNORE_ENTRIES{"/bb/", "/compile_commands.json"};

std::expected<std::string, std::string> read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::unexpected("Could not open " + path.string());
    }

    std::string contents{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (input.bad()) {
        return std::unexpected("Could not read " + path.string());
    }

    return contents;
}

// Writes through a temporary file, so a failure leaves any existing file intact.
std::expected<void, std::string> replace_file(const std::filesystem::path& path,
                                              std::string_view contents) {
    auto temporary = path;
    temporary += ".tmp";

    std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
    if (!output) {
        return std::unexpected("Could not open " + temporary.string());
    }

    output << contents;
    if (auto result = finish_file(output, temporary); !result) {
        return result;
    }

    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::string diagnostic = "Could not replace " + path.string() + ": " + error.message();
        remove_created_path(temporary, diagnostic);
        return std::unexpected(diagnostic);
    }

    return {};
}

bool has_line(std::string_view contents, std::string_view line) {
    for (const auto part : std::views::split(contents, '\n')) {
        if (string_utils::trim(std::string_view{part.begin(), part.end()}) == line) {
            return true;
        }
    }

    return false;
}

// Adds bb's entries to .gitignore, creating the file if needed.
std::expected<void, std::string> update_gitignore_file(const std::filesystem::path& gitignore_file) {
    std::error_code error;
    const bool exists = std::filesystem::exists(gitignore_file, error);
    if (error) {
        return std::unexpected("Could not inspect " + gitignore_file.string() + ": " +
                               error.message());
    }

    std::string previous;
    if (exists) {
        auto contents = read_file(gitignore_file);
        if (!contents) {
            return std::unexpected(contents.error());
        }
        previous = std::move(*contents);
    }

    std::string updated = previous;
    for (const auto entry : GITIGNORE_ENTRIES) {
        if (has_line(updated, entry)) {
            continue;
        }

        if (!updated.empty() && !updated.ends_with('\n')) {
            updated += '\n';
        }
        updated += entry;
        updated += '\n';
    }

    if (updated == previous) {
        return {};
    }

    return replace_file(gitignore_file, updated);
}

// Running git init inside an existing repository would create a nested one.
bool inside_git_work_tree(const std::filesystem::path& directory) {
    auto result = bb::run_process_capture(
        {"git", "-C", directory.string(), "rev-parse", "--is-inside-work-tree"});
    return result && result->exit_code == 0 && string_utils::trim(result->output) == "true";
}

// Written before the first build so clangd works right away. The generated build.cpp doesn't
// set a standard, so src/main.cpp gets the flags the runner will use for Build's default.
std::expected<void, std::string> write_compile_commands(const std::filesystem::path& project_dir) {
    const auto flags = bb::target_flags(bb::Build{}.cpp_standard(), {});
    const std::vector<bb::CompileCommand> commands{
        bb::build_configuration_command(project_dir),
        bb::source_command(project_dir, flags, "src/main.cpp"),
    };

    return bb::write_compilation_database(project_dir / "compile_commands.json", commands,
                                          bb::DatabaseWriteMode::CREATE);
}

} // namespace

std::expected<void, std::string> bb::init() {
    namespace fs = std::filesystem;

    std::error_code error;
    const auto project_dir = fs::current_path(error);

    if (error) {
        return std::unexpected("Could not determine project directory: " + error.message());
    }

    const auto build_file = project_dir / "build.cpp";
    const auto source_dir = project_dir / "src";
    const auto main_file = source_dir / "main.cpp";
    const auto commands_file = project_dir / "compile_commands.json";

    const auto gitignore_file = project_dir / ".gitignore";

    // An existing .gitignore is fine (e.g. a freshly cloned repository): bb adds its entries.
    for (const auto& path : {build_file, main_file, commands_file}) {
        const bool exists = fs::exists(path, error);

        if (error) {
            return std::unexpected("Could not inspect " + path.string() + ": " + error.message());
        }

        if (exists) {
            return std::unexpected("File already exists: " + path.string());
        }
    }

    const bool created_source_dir = fs::create_directories(source_dir, error);

    if (error) {
        return std::unexpected("Could not create source directory: " + error.message());
    }

    std::vector<fs::path> created_files;
    const auto rollback = [&](std::string diagnostic) -> std::expected<void, std::string> {
        for (auto it = created_files.rbegin(); it != created_files.rend(); ++it) {
            remove_created_path(*it, diagnostic);
        }
        if (created_source_dir) {
            // remove() leaves nonempty directories intact.
            remove_created_path(source_dir, diagnostic);
        }
        return std::unexpected(diagnostic);
    };

    if (auto result = write_main_file(main_file); !result) {
        return rollback(result.error());
    }
    created_files.push_back(main_file);

    if (auto result = write_build_file(build_file); !result) {
        return rollback(result.error());
    }
    created_files.push_back(build_file);

    if (auto result = write_compile_commands(project_dir); !result) {
        return rollback(result.error());
    }
    created_files.push_back(commands_file);

    for (const auto& [name, contents] : {std::pair{".clang-format", CLANG_FORMAT},
                                         std::pair{".clang-tidy", CLANG_TIDY}}) {
        const auto path = project_dir / name;
        auto created = write_config_file(path, contents);
        if (!created) {
            return rollback(created.error());
        }
        if (*created) {
            created_files.push_back(path);
        }
    }

    // Last, so rollback never has to restore an existing .gitignore. A failed update leaves
    // the file untouched.
    if (auto result = update_gitignore_file(gitignore_file); !result) {
        return rollback(result.error());
    }

    // File generation is complete: a Git failure must preserve the project.
    if (inside_git_work_tree(project_dir)) {
        return {};
    }

    const std::string git_retry = "\nRun git init in this directory to retry.";
    auto result = run_process({"git", "-C", project_dir.string(), "init"});
    if (!result) {
        return std::unexpected("Project files were created, but Git initialization failed: " +
                               result.error() + git_retry);
    }

    if (*result != 0) {
        return std::unexpected("Project files were created, but git init failed with exit code " +
                               std::to_string(*result) +
                               (*result == 127 ? " (Git may be unavailable in PATH)" : "") +
                               git_retry);
    }

    return {};
}
