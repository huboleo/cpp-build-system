#include "executable_search.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

bool is_executable_file(const fs::path& path) {
    std::error_code error;
    return fs::is_regular_file(path, error) && !error && access(path.c_str(), X_OK) == 0;
}

std::expected<fs::path, std::string> absolute_path(const fs::path& path) {
    std::error_code error;
    auto absolute = fs::absolute(path, error);
    if (error) {
        return std::unexpected("Cannot resolve " + path.string() + ": " + error.message());
    }
    return absolute;
}

} // namespace

std::expected<fs::path, std::string> bb::find_executable(std::string_view executable) {
    const fs::path requested{executable};

    if (requested.has_parent_path()) {
        if (!is_executable_file(requested)) {
            return std::unexpected("Not an executable file: " + requested.string());
        }

        return absolute_path(requested);
    }

    const char* path_environment = std::getenv("PATH");
    if (path_environment == nullptr) {
        return std::unexpected("Cannot find " + requested.string() + " because PATH is not set");
    }

    const std::string_view search_path{path_environment};
    std::size_t begin = 0;
    while (begin <= search_path.size()) {
        const auto end = search_path.find(':', begin);
        const auto directory = search_path.substr(begin, end - begin);
        const fs::path candidate = directory.empty() ? requested : fs::path{directory} / requested;

        if (is_executable_file(candidate)) {
            return absolute_path(candidate);
        }

        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }

    return std::unexpected("Cannot find " + requested.string() + " in PATH");
}
