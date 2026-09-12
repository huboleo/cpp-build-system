#include "initializer.hpp"
#include "compilation_database.hpp"
#include "process.hpp"
#include <expected>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
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

int main()
{
    std::println("Hello!");
    return 0;
}
    )";

    return finish_file(main_output, main_file);
}

std::expected<void, std::string> write_build_file(
    const std::filesystem::path& build_file,
    bb::InitMode mode = bb::InitMode::new_project) {
    std::ofstream build_output{build_file, std::ios::out | std::ios::noreplace};

    if (!build_output) {
        return std::unexpected("Could not open " + build_file.string());
    }

    if (mode == bb::InitMode::existing_project) {
        build_output << R"(#include <bb/build.hpp>

void build(bb::Build& b)
{
    // List the source files for your executable:
    // b.executable("app", {"src/main.cpp", "src/utils.cpp"});
}
)";
    } else {
        build_output << R"(#include <bb/build.hpp>

void build(bb::Build& b)
{
    b.executable("app", {"src/main.cpp"});
}
    )";
    }

    return finish_file(build_output, build_file);
}

std::expected<void, std::string> write_gitignore_file(const std::filesystem::path& gitignore_file) {
    std::ofstream output{gitignore_file, std::ios::out | std::ios::noreplace};

    if (!output) {
        return std::unexpected("Could not open " + gitignore_file.string());
    }

    output << R"(.cache
build
)";
    return finish_file(output, gitignore_file);
}

std::expected<void, std::string> write_compile_commands(
    const std::filesystem::path& project_dir,
    bb::InitMode mode = bb::InitMode::new_project) {
    std::vector<bb::CompileCommand> commands{
        bb::build_configuration_command(project_dir)
    };

    if (mode == bb::InitMode::new_project) {
        commands.push_back({project_dir, "src/main.cpp",
                            {"clang++", "-std=c++23", "-c", "src/main.cpp"}});
    }

    return bb::write_compilation_database(
        project_dir / "compile_commands.json", commands,
        mode == bb::InitMode::existing_project ? bb::DatabaseWriteMode::merge
                                               : bb::DatabaseWriteMode::create);
}

std::expected<void, std::string> init_existing(const std::filesystem::path& project_dir) {
    const auto build_file = project_dir / "build.cpp";
    const auto gitignore_file = project_dir / ".gitignore";
    std::error_code error;
    const bool has_gitignore = std::filesystem::exists(gitignore_file, error);
    if (error) {
        return std::unexpected("Could not inspect " + gitignore_file.string() + ": " + error.message());
    }

    if (auto result = write_build_file(build_file, bb::InitMode::existing_project); !result) {
        return result;
    }

    bool created_gitignore = false;
    const auto rollback = [&](std::string diagnostic) -> std::expected<void, std::string> {
        if (created_gitignore) {
            remove_created_path(gitignore_file, diagnostic);
        }
        remove_created_path(build_file, diagnostic);
        return std::unexpected(diagnostic);
    };

    if (!has_gitignore) {
        if (auto result = write_gitignore_file(gitignore_file); !result) {
            return rollback(result.error());
        }
        created_gitignore = true;
    }

    // Update the existing database last, after all other file creation succeeds.
    if (auto result = write_compile_commands(project_dir, bb::InitMode::existing_project); !result) {
        return rollback(result.error());
    }

    return {};
}

} // namespace

std::expected<void, std::string> bb::init(InitMode mode) {
    namespace fs = std::filesystem;

    std::error_code error;
    const auto project_dir = fs::current_path(error);

    if (error) {
        return std::unexpected("Could not determine project directory: " + error.message());
    }

    if (mode == InitMode::existing_project) {
        return init_existing(project_dir);
    }

    const auto build_file = project_dir / "build.cpp";
    const auto source_dir = project_dir / "src";
    const auto main_file = source_dir / "main.cpp";
    const auto commands_file = project_dir / "compile_commands.json";

    const auto gitignore_file = project_dir / ".gitignore";

    for (const auto& path : {build_file, main_file, commands_file, gitignore_file}) {
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

    if (auto result = write_gitignore_file(gitignore_file); !result) {
        return rollback(result.error());
    }
    created_files.push_back(gitignore_file);

    // File generation is complete: a Git failure must preserve the project.
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
