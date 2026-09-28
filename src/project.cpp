#include "project.hpp"

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

std::expected<fs::path, std::string> bb::find_project_root(const fs::path& start) {
    std::error_code error;

    for (auto directory = start;; directory = directory.parent_path()) {
        const auto build_file = directory / "build.cpp";
        if (fs::exists(build_file, error)) {
            return directory;
        }

        if (error) {
            return std::unexpected("Cannot inspect " + build_file.string() + ": " +
                                   error.message());
        }

        if (directory == directory.parent_path()) {
            return std::unexpected("Cannot find build.cpp in this directory or any parent; "
                                   "run bb init to create a project");
        }
    }
}
