#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bb {
enum class CppStandard {
    CPP_11,
    CPP_14,
    CPP_17,
    CPP_20,
    CPP_23,
};

struct Executable {
    std::string name;
    std::vector<std::string> sources;
};

enum class BuildError : uint8_t { EXECUTABLE_ALREADY_DECLARED };

class Build {
  public:
    void executable(std::string_view name, std::initializer_list<std::string_view> sources);
    [[nodiscard]] const std::optional<Executable>& target() const;
    [[nodiscard]] std::optional<BuildError> error() const;
    [[nodiscard]] CppStandard cpp_standard() const;
    void set_cpp_standard(CppStandard standard);

  private:
    std::optional<Executable> _target;
    std::optional<BuildError> _error;
    CppStandard _cpp_standard{CppStandard::CPP_23};
};
} // namespace bb

void build(bb::Build& b);
