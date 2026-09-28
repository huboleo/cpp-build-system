#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bb {
enum class CppStandard {
    CPP_11,
    CPP_14,
    CPP_17,
    CPP_20,
    CPP_23,
};

// Handles returned by Build. Copies refer to the same target, and only Build can create one,
// so every handle points at a target that exists.
class Executable {
  private:
    friend class Build;
    explicit Executable(std::uint32_t index) : _index{index} {}
    std::uint32_t _index;
};

class Library {
  public:
    // Turns `.links = {some_executable}` into a readable compile error.
    Library(Executable) = delete("an executable can't be linked; link a library instead");

  private:
    friend class Build;
    explicit Library(std::uint32_t index) : _index{index} {}
    std::uint32_t _index;
};

// Designated initializers must list fields in declaration order, so new fields only go at the
// end. Optional fields default to {} so leaving them out doesn't warn; name has no default, so
// leaving it out does.
struct LibraryOptions {
    std::string name;
    std::vector<std::string> sources = {};
    std::vector<std::string> include_paths = {};        // used only by this library's sources
    std::vector<std::string> public_include_paths = {}; // also used by everything that links it
    std::vector<Library> links = {};
};

struct ExecutableOptions {
    std::string name;
    std::vector<std::string> sources = {};
    std::vector<std::string> include_paths = {};
    std::vector<Library> links = {};
};

namespace detail {
struct BuildAccess; // defined in bb's own sources, not part of the API
} // namespace detail

class Build {
  public:
    Library library(LibraryOptions options);
    Executable executable(ExecutableOptions options);

    [[nodiscard]] CppStandard cpp_standard() const;
    void set_cpp_standard(CppStandard standard);

  private:
    friend struct detail::BuildAccess;

    [[nodiscard]] const LibraryOptions& resolve(Library library) const;

    std::vector<LibraryOptions> _libraries;
    std::vector<ExecutableOptions> _executables;
    std::vector<std::string> _errors;
    CppStandard _cpp_standard{CppStandard::CPP_23};
};
} // namespace bb

void build(bb::Build& b);
