#include "executable_search.hpp"
#include "test_support.hpp"

#include <gtest/gtest.h>
#include <string>

using bb::test::ScopedEnvironment;
using bb::test::TempDir;
using bb::test::write_executable;

TEST(FindExecutable, SearchesPathInOrder) {
    TempDir dir;
    write_executable(dir / "first/tool", "#!/bin/sh\n");
    write_executable(dir / "second/tool", "#!/bin/sh\n");
    ScopedEnvironment path{"PATH", (dir / "missing").string() + ":" + (dir / "first").string() +
                                       ":" + (dir / "second").string()};

    const auto found = bb::find_executable("tool");
    ASSERT_TRUE(found) << found.error();
    EXPECT_EQ(*found, dir / "first/tool");
}

TEST(FindExecutable, SkipsFilesThatAreNotExecutable) {
    TempDir dir;
    bb::test::write_file(dir / "first/tool", "");
    write_executable(dir / "second/tool", "#!/bin/sh\n");
    ScopedEnvironment path{"PATH", (dir / "first").string() + ":" + (dir / "second").string()};

    const auto found = bb::find_executable("tool");
    ASSERT_TRUE(found) << found.error();
    EXPECT_EQ(*found, dir / "second/tool");
}

TEST(FindExecutable, MakesRelativePathsAbsolute) {
    TempDir dir;
    write_executable(dir / "bin/tool", "#!/bin/sh\n");
    bb::test::ScopedCurrentPath current{dir.path()};

    const auto found = bb::find_executable("bin/tool");
    ASSERT_TRUE(found) << found.error();
    EXPECT_EQ(*found, dir / "bin/tool");
}

TEST(FindExecutable, ReportsMissingPrograms) {
    TempDir dir;
    ScopedEnvironment path{"PATH", dir.path().string()};

    const auto from_path = bb::find_executable("bb-test-missing");
    ASSERT_FALSE(from_path);
    EXPECT_NE(from_path.error().find("in PATH"), std::string::npos) << from_path.error();

    EXPECT_FALSE(bb::find_executable((dir / "missing").string()));
}
