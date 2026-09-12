#include "compilation_database.hpp"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <system_error>
#include <utility>

namespace {

using nlohmann::json;
namespace fs = std::filesystem;

bool valid_entry(const json& entry) {
    if (!entry.is_object() || !entry.contains("directory") ||
        !entry["directory"].is_string() || !entry.contains("file") ||
        !entry["file"].is_string()) {
        return false;
    }

    if (!fs::path{entry["directory"].get<std::string>()}.is_absolute()) {
        return false;
    }

    if (entry.contains("arguments")) {
        const auto& arguments = entry["arguments"];
        return arguments.is_array() && !arguments.empty() &&
               std::ranges::all_of(arguments, [](const json& value) { return value.is_string(); });
    }

    return entry.contains("command") && entry["command"].is_string();
}

fs::path source_path(const json& entry) {
    return (fs::path{entry["directory"].get<std::string>()} /
            entry["file"].get<std::string>()).lexically_normal();
}

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

} // namespace

bb::CompileCommand bb::build_configuration_command(const std::filesystem::path& project_dir) {
    return {
        project_dir,
        "build.cpp",
        {"clang++", "-std=c++23", "-I",
         "/Users/hubert/Projects/cpp-build-system/include", "-c", "build.cpp"}
    };
}

std::expected<void, std::string> bb::write_compilation_database(
    const std::filesystem::path& path,
    std::span<const CompileCommand> commands,
    DatabaseWriteMode mode) {
    try {
        auto database = json::array();

        if (mode == DatabaseWriteMode::merge) {
            std::error_code error;
            const bool exists = fs::exists(path, error);
            if (error) {
                return std::unexpected("Could not inspect " + path.string() + ": " + error.message());
            }

            if (exists) {
                if (!fs::is_regular_file(path, error)) {
                    return std::unexpected("Cannot read compilation database " + path.string() +
                                           (error ? ": " + error.message() : ": not a regular file"));
                }
                std::ifstream input{path, std::ios::binary};
                if (!input) {
                    return std::unexpected("Could not open " + path.string());
                }

                database = json::parse(input, nullptr, false);
                if (input.bad()) {
                    return std::unexpected("Could not read " + path.string());
                }
                if (database.is_discarded()) {
                    return std::unexpected("Invalid JSON in " + path.string());
                }
                if (!database.is_array() || !std::ranges::all_of(database, valid_entry)) {
                    return std::unexpected("Invalid compilation database entries in " + path.string());
                }
            }
        }

        for (const auto& command : commands) {
            json entry{
                {"directory", command.directory.string()},
                {"file", command.file.string()},
                {"arguments", command.arguments}
            };
            if (!valid_entry(entry)) {
                return std::unexpected("Invalid compile command for " + command.file.string());
            }

            if (mode == DatabaseWriteMode::merge) {
                const auto source = source_path(entry);
                // Match relative and absolute spellings of the same source.
                for (auto it = database.begin(); it != database.end();) {
                    if (source_path(*it) == source) {
                        it = database.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            database.push_back(std::move(entry));
        }

        // Serialize before opening any output file, so JSON errors change nothing.
        const auto contents = database.dump(2) + "\n";
        if (mode == DatabaseWriteMode::create) {
            return write_file(path, contents);
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
