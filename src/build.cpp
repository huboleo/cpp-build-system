#include "bb/build.hpp"
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

void bb::Build::executable(std::string_view name, std::initializer_list<std::string_view> sources) {
    if (_error) {
        return;
    }

    if (_target) {
        _error = BuildError::EXECUTABLE_ALREADY_DECLARED;
        return;
    }

    Executable exec{.name = std::string(name), .sources = {}};
    exec.sources.reserve(sources.size());
    for (const auto& source : sources) {
        exec.sources.emplace_back(source);
    }

    _target = std::move(exec);
}

const std::optional<bb::Executable>& bb::Build::target() const { return _target; }

std::optional<bb::BuildError> bb::Build::error() const { return _error; }
