#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bb {
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

  private:
    std::optional<Executable> _target;
    std::optional<BuildError> _error;
};
} // namespace bb

void build(bb::Build& b);
