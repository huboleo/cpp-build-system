#include "compilation_database.hpp"
#include "compile_flags.hpp"

#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <system_error>
#include <utility>

namespace {

using nlohmann::json;
namespace fs = std::filesystem;

std::expected<void, std::string> write_file(const fs::path& path, const std::string& contents) {
    std::ofstream output{path, std::ios::binary | std::ios::noreplace};
    if (!output) {
        return std::unexpected("Could not create " + path.string());
    }

    output << contents;
    output.close();
    if (!output) {
        std::string diagnostic = "Could not write " + path.string();
        std::error_code error;
        fs::remove(path, error);
        if (error) {
            diagnostic += "\nCleanup failed: " + error.message();
        }
        return std::unexpected(diagnostic);
    }

    return {};
}

// A missing or unreadable file counts as different.
bool has_contents(const fs::path& path, const std::string& contents) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return false;
    }

    const std::string existing{std::istreambuf_iterator<char>{input},
                               std::istreambuf_iterator<char>{}};
    return !input.bad() && existing == contents;
}

} // namespace

bb::CompileCommand bb::source_command(const std::filesystem::path& project_dir,
                                      const std::vector<std::string>& flags,
                                      const std::string& source) {
    std::vector<std::string> arguments{"clang++"};
    arguments.insert(arguments.end(), flags.begin(), flags.end());
    arguments.emplace_back("-c");
    arguments.push_back(source);

    return {.directory = project_dir, .file = source, .arguments = std::move(arguments)};
}

bb::CompileCommand bb::build_configuration_command(const std::filesystem::path& project_dir) {
    return source_command(project_dir, build_file_flags(), "build.cpp");
}

std::expected<void, std::string>
bb::write_compilation_database(const std::filesystem::path& path,
                               std::span<const CompileCommand> commands, DatabaseWriteMode mode) {
    try {
        auto database = json::array();

        for (const auto& command : commands) {
            json entry{{"directory", command.directory.string()},
                       {"file", command.file.string()},
                       {"arguments", command.arguments}};
            database.push_back(std::move(entry));
        }

        // Serialize before opening any output file, so JSON errors change nothing.
        const auto contents = database.dump(2) + "\n";
        if (mode == DatabaseWriteMode::CREATE) {
            return write_file(path, contents);
        }

        if (has_contents(path, contents)) {
            return {};
        }

        auto temporary = path;
        temporary += ".tmp";
        if (auto result = write_file(temporary, contents); !result) {
            return result;
        }

        // Keep the previous database intact until the complete new file is ready.
        std::error_code error;
        fs::rename(temporary, path, error);
        if (error) {
            std::string diagnostic = "Could not replace " + path.string() + ": " + error.message();
            fs::remove(temporary, error);
            if (error) {
                diagnostic += "\nCleanup failed: " + error.message();
            }
            return std::unexpected(diagnostic);
        }

        return {};
    } catch (const json::exception& error) {
        // Keep library errors behind bb's errors-as-values interface.
        return std::unexpected("Compilation database JSON error: " + std::string{error.what()});
    } catch (const std::ios_base::failure& error) {
        return std::unexpected("Compilation database I/O error: " + std::string{error.what()});
    }
}
