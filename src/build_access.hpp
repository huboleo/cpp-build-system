#pragma once

#include "bb/build.hpp"

#include <vector>

namespace bb::detail {

// Gives bb's own code (the runner) read access to what build() recorded, without making it
// part of the public API.
struct BuildAccess {
    static const std::vector<LibraryOptions>& libraries(const Build& b) { return b._libraries; }

    static const std::vector<ExecutableOptions>& executables(const Build& b) {
        return b._executables;
    }

    static const LibraryOptions& resolve(const Build& b, Library library) {
        return b.resolve(library);
    }

    static const auto& errors(const Build& b) { return b._errors; }
};

} // namespace bb::detail
