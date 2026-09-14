#include "dependency_file.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::optional<std::size_t> find_dependency_separator(std::string_view contents) {
    bool escaped = false;

    for (std::size_t i = 0; i < contents.size(); ++i) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (contents[i] == '\\') {
            escaped = true;
        } else if (contents[i] == ':') {
            return i;
        }
    }

    return std::nullopt;
}

void append_dependency(std::vector<std::filesystem::path>& dependencies, std::string& current) {
    if (current.empty()) {
        return;
    }

    dependencies.emplace_back(std::move(current));
    current.clear();
}

} // namespace

std::expected<std::vector<std::filesystem::path>, std::string>
bb::read_dependency_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::unexpected("Cannot open dependency file: " + path.string());
    }

    const std::string contents{std::istreambuf_iterator<char>{input},
                               std::istreambuf_iterator<char>{}};
    if (input.bad()) {
        return std::unexpected("Cannot read dependency file: " + path.string());
    }

    const auto separator = find_dependency_separator(contents);
    if (!separator) {
        return std::unexpected("Malformed dependency file: " + path.string());
    }

    std::vector<std::filesystem::path> dependencies;
    std::string current;

    for (std::size_t i = *separator + 1; i < contents.size(); ++i) {
        const char character = contents[i];

        if (character == '\\') {
            if (i + 1 >= contents.size()) {
                return std::unexpected("Malformed dependency file: " + path.string());
            }

            if (contents[i + 1] == '\n') {
                ++i;
                continue;
            }

            if (contents[i + 1] == '\r' && i + 2 < contents.size() &&
                contents[i + 2] == '\n') {
                i += 2;
                continue;
            }

            current.push_back(contents[++i]);
            continue;
        }

        if (character == '$' && i + 1 < contents.size() && contents[i + 1] == '$') {
            current.push_back('$');
            ++i;
            continue;
        }

        if (character == '#') {
            append_dependency(dependencies, current);
            while (i + 1 < contents.size() && contents[i + 1] != '\n') {
                ++i;
            }
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(character))) {
            append_dependency(dependencies, current);
            continue;
        }

        current.push_back(character);
    }

    append_dependency(dependencies, current);
    return dependencies;
}
