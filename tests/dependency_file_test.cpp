#include "dependency_file.hpp"
#include "test_support.hpp"

#include <expected>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Paths = std::vector<std::string>;

std::expected<Paths, std::string> parse(std::string_view contents) {
    bb::test::TempDir dir;
    bb::test::write_file(dir / "deps.d", contents);

    auto dependencies = bb::read_dependency_file(dir / "deps.d");
    if (!dependencies) {
        return std::unexpected(dependencies.error());
    }

    Paths paths;
    for (const auto& dependency : *dependencies) {
        paths.push_back(dependency.string());
    }
    return paths;
}

} // namespace

TEST(DependencyFile, ListsEverythingAfterTheTarget) {
    const auto paths = parse("build-runner: build.cpp /sdk/bb/build.hpp\n");
    ASSERT_TRUE(paths) << paths.error();
    EXPECT_EQ(*paths, (Paths{"build.cpp", "/sdk/bb/build.hpp"}));
}

TEST(DependencyFile, FollowsLineContinuations) {
    const auto paths = parse("out: a.cpp \\\n  b.h \\\r\n  c.h\n");
    ASSERT_TRUE(paths) << paths.error();
    EXPECT_EQ(*paths, (Paths{"a.cpp", "b.h", "c.h"}));
}

TEST(DependencyFile, UnescapesSpacesAndDollarSigns) {
    const auto paths = parse("out: my\\ dir/a.h cost$$.h\n");
    ASSERT_TRUE(paths) << paths.error();
    EXPECT_EQ(*paths, (Paths{"my dir/a.h", "cost$.h"}));
}

TEST(DependencyFile, SkipsComments) {
    const auto paths = parse("out: a.h # written by clang\n");
    ASSERT_TRUE(paths) << paths.error();
    EXPECT_EQ(*paths, (Paths{"a.h"}));
}

TEST(DependencyFile, EscapedColonInTheTargetIsNotTheSeparator) {
    const auto paths = parse("dir\\:name/out: a.h\n");
    ASSERT_TRUE(paths) << paths.error();
    EXPECT_EQ(*paths, (Paths{"a.h"}));
}

TEST(DependencyFile, RejectsMalformedFiles) {
    for (const std::string_view contents : {"a.h b.h\n", "out: a.h \\"}) {
        const auto paths = parse(contents);
        ASSERT_FALSE(paths) << contents;
        EXPECT_NE(paths.error().find("Malformed"), std::string::npos) << paths.error();
    }
}

TEST(DependencyFile, MissingFileIsAnError) {
    bb::test::TempDir dir;
    EXPECT_FALSE(bb::read_dependency_file(dir / "missing.d"));
}

// Known bug: on Windows clang writes paths with plain backslashes, and the parser treats every
// backslash as an escape. Enable this test when fixing the parser for the Windows port.
TEST(DependencyFile, DISABLED_KeepsWindowsBackslashPaths) {
    const auto paths = parse("C:\\proj\\out.o: C:\\proj\\src\\main.cpp\n");
    ASSERT_TRUE(paths) << paths.error();
    EXPECT_EQ(*paths, (Paths{"C:\\proj\\src\\main.cpp"}));
}
