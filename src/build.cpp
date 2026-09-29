#include "bb/build.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

// Files that are compiled on their own. Headers are left out: they come in through #include,
// and passing one to the compiler makes it try to build a precompiled header. Module files
// (.cppm, .ixx) and C files wait until bb can build them.
const std::vector<std::string_view> SOURCE_EXTENSIONS{".cc", ".c++", ".cpp", ".cxx"};

// Errors have no line numbers, so each message names the target it is about: by its name, or
// by its first source if it has no name.
std::optional<std::string> name_error(std::string_view kind, const std::string& name,
                                      const std::vector<std::string>& sources,
                                      const std::vector<bb::LibraryOptions>& libraries,
                                      const std::vector<bb::ExecutableOptions>& executables) {
    if (name.empty()) {
        if (sources.empty()) {
            return std::format("{} has no name and no sources", kind);
        }
        return std::format("{} with source '{}' has no name", kind, sources.front());
    }

    // The name becomes the output file name, so it must not point into another directory.
    if (name.contains('/') || name.contains('\\')) {
        return std::format("{} '{}': name must not contain '/' or '\\'", kind, name);
    }

    if (name == "." || name == "..") {
        return std::format("{} '{}': name is not a valid file name", kind, name);
    }

    const auto same_name = [&](const auto& target) {
        return target.name == name;
    };
    if (std::ranges::any_of(libraries, same_name) || std::ranges::any_of(executables, same_name)) {
        return std::format("{} '{}': name is already used by another target", kind, name);
    }

    return std::nullopt;
}

} // namespace

bb::Library bb::Build::library(bb::LibraryOptions options) {
    if (auto error =
            name_error("library", options.name, options.sources, _libraries, _executables)) {
        _errors.push_back(std::move(*error));
    }

    _libraries.push_back(std::move(options));
    return Library{static_cast<std::uint32_t>(_libraries.size() - 1)};
}

bb::Executable bb::Build::executable(bb::ExecutableOptions options) {
    if (auto error =
            name_error("executable", options.name, options.sources, _libraries, _executables)) {
        _errors.push_back(std::move(*error));
    }

    _executables.push_back(std::move(options));
    return Executable{static_cast<std::uint32_t>(_executables.size() - 1)};
}

void bb::Build::set_cpp_standard(bb::CppStandard standard) {
    _cpp_standard = standard;
}

bb::CppStandard bb::Build::cpp_standard() const {
    return _cpp_standard;
}

const bb::LibraryOptions& bb::Build::resolve(bb::Library library) const {
    return _libraries[library._index];
}

std::vector<std::string>
bb::Build::all_sources_from(const std::filesystem::path& path,
                            const std::vector<std::string_view>& extensions) {
    std::vector<std::string> result;
    std::error_code error;
    std::filesystem::recursive_directory_iterator entry{path, error};
    const std::filesystem::recursive_directory_iterator end;

    for (; !error && entry != end; entry.increment(error)) {
        if (entry->is_regular_file() &&
            std::ranges::contains(extensions, entry->path().extension().string())) {
            result.push_back(entry->path());
        }
    }

    if (error) {
        _errors.push_back(
            std::format("cannot list sources in '{}': {}", path.string(), error.message()));
        return {};
    }

    return result;
}

std::vector<std::string> bb::Build::all_sources_from(const std::filesystem::path& path) {
    return all_sources_from(path, SOURCE_EXTENSIONS);
}
