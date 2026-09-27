#include "bb/build.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

namespace {

std::optional<std::string> name_error(const bb::Build& b, const std::string& name) {
    if (name.empty()) {
        return "target name must not be empty";
    }

    const auto same_name = [&](const auto& target) { return target.name == name; };
    if (std::ranges::any_of(b.libraries(), same_name) ||
        std::ranges::any_of(b.executables(), same_name)) {
        return "target '" + name + "' is already declared";
    }

    return std::nullopt;
}

} // namespace

bb::Library bb::Build::library(bb::LibraryOptions options, std::source_location location) {
    if (auto error = name_error(*this, options.name)) {
        _errors.push_back({std::move(*error), location});
    }

    _libraries.push_back(std::move(options));
    return Library{static_cast<std::uint32_t>(_libraries.size() - 1)};
}

bb::Executable bb::Build::executable(bb::ExecutableOptions options,
                                     std::source_location location) {
    if (auto error = name_error(*this, options.name)) {
        _errors.push_back({std::move(*error), location});
    }

    _executables.push_back(std::move(options));
    return Executable{static_cast<std::uint32_t>(_executables.size() - 1)};
}

void bb::Build::set_cpp_standard(bb::CppStandard standard) { _cpp_standard = standard; }

bb::CppStandard bb::Build::cpp_standard() const { return _cpp_standard; }

const std::vector<bb::LibraryOptions>& bb::Build::libraries() const { return _libraries; }

const std::vector<bb::ExecutableOptions>& bb::Build::executables() const { return _executables; }

const bb::LibraryOptions& bb::Build::resolve(bb::Library library) const {
    return _libraries[library._index];
}

const std::vector<bb::BuildError>& bb::Build::errors() const { return _errors; }
