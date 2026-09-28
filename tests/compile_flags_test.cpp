#include "compile_flags.hpp"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

using Flags = std::vector<std::string>;

TEST(CompileFlags, TargetFlagsSelectTheStandard) {
    const std::vector<std::pair<bb::CppStandard, std::string>> standards{
        {bb::CppStandard::CPP_11, "-std=c++11"}, {bb::CppStandard::CPP_14, "-std=c++14"},
        {bb::CppStandard::CPP_17, "-std=c++17"}, {bb::CppStandard::CPP_20, "-std=c++20"},
        {bb::CppStandard::CPP_23, "-std=c++23"},
    };

    for (const auto& [standard, flag] : standards) {
        EXPECT_EQ(bb::target_flags(standard, {}), Flags{flag});
    }
}

TEST(CompileFlags, TargetFlagsAddIncludePathsInOrder) {
    EXPECT_EQ(bb::target_flags(bb::CppStandard::CPP_20, {"include", "third_party/lib"}),
              (Flags{"-std=c++20", "-I", "include", "-I", "third_party/lib"}));
}

TEST(CompileFlags, BuildFileWarningsSkipBbHeaders) {
    EXPECT_EQ(bb::build_file_flags(), (Flags{"-std=c++23", "-pedantic", "-Wall", "-Wextra",
                                             "-isystem", BB_SDK_INCLUDE_DIR}));
}

TEST(CompileFlags, SdkHeadersIncludeBuildHpp) {
    const auto headers = bb::sdk_headers();
    ASSERT_TRUE(headers) << headers.error();

    const auto build_hpp = std::filesystem::path{BB_SDK_INCLUDE_DIR} / "bb" / "build.hpp";
    EXPECT_NE(std::ranges::find(*headers, build_hpp), headers->end());
}
