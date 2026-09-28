#include "compile_flags.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

std::string_view cpp_standard_enum_to_string(bb::CppStandard standard) {
    switch (standard) {
    case bb::CppStandard::CPP_11:
        return "-std=c++11";
    case bb::CppStandard::CPP_14:
        return "-std=c++14";
    case bb::CppStandard::CPP_17:
        return "-std=c++17";
    case bb::CppStandard::CPP_20:
        return "-std=c++20";
    case bb::CppStandard::CPP_23:
        return "-std=c++23";
    }
    return "-std=c++23";
}

} // namespace

std::vector<std::string> bb::build_file_flags() {
    return {"-std=c++23", "-pedantic", "-Wall", "-Wextra", "-isystem", BB_SDK_INCLUDE_DIR};
}

std::expected<std::vector<std::filesystem::path>, std::string> bb::sdk_headers() {
    namespace fs = std::filesystem;

    std::vector<fs::path> headers;
    std::error_code error;
    fs::recursive_directory_iterator entry{BB_SDK_INCLUDE_DIR, error};
    const fs::recursive_directory_iterator end;

    for (; !error && entry != end; entry.increment(error)) {
        if (entry->is_regular_file(error)) {
            headers.push_back(entry->path());
        }
    }

    if (error) {
        return std::unexpected(std::string{"Cannot list bb headers in "} + BB_SDK_INCLUDE_DIR +
                               ": " + error.message());
    }

    return headers;
}

std::vector<std::string> bb::target_flags(CppStandard standard,
                                          const std::vector<std::string>& include_paths) {
    std::vector<std::string> flags{std::string{cpp_standard_enum_to_string(standard)}};
    for (const auto& include : include_paths) {
        flags.emplace_back("-I");
        flags.push_back(include);
    }
    return flags;
}
