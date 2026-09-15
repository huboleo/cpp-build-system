#include "bb/build.hpp"
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

void bb::Build::executable(bb::Executable executable) {
    if (_error) {
        return;
    }

    if (_target) {
        _error = BuildError::EXECUTABLE_ALREADY_DECLARED;
        return;
    }

    _target = std::move(executable);
}

const std::optional<bb::Executable>& bb::Build::target() const { return _target; }

std::optional<bb::BuildError> bb::Build::error() const { return _error; }

void bb::Build::set_cpp_standard(bb::CppStandard standard) { _cpp_standard = standard; }

bb::CppStandard bb::Build::cpp_standard() const { return _cpp_standard; }
