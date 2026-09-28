#include "project.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

using bb::test::TempDir;
using bb::test::write_file;

TEST(ProjectRoot, IsTheDirectoryWithBuildCpp) {
    TempDir dir;
    write_file(dir / "build.cpp", "");

    const auto root = bb::find_project_root(dir.path());
    ASSERT_TRUE(root) << root.error();
    EXPECT_EQ(*root, dir.path());
}

TEST(ProjectRoot, IsFoundFromASubdirectory) {
    TempDir dir;
    write_file(dir / "build.cpp", "");
    std::filesystem::create_directories(dir / "src/nested");

    const auto root = bb::find_project_root(dir / "src/nested");
    ASSERT_TRUE(root) << root.error();
    EXPECT_EQ(*root, dir.path());
}

TEST(ProjectRoot, NearestBuildCppWins) {
    TempDir dir;
    write_file(dir / "build.cpp", "");
    write_file(dir / "tools/generator/build.cpp", "");
    std::filesystem::create_directories(dir / "tools/generator/src");

    const auto root = bb::find_project_root(dir / "tools/generator/src");
    ASSERT_TRUE(root) << root.error();
    EXPECT_EQ(*root, dir / "tools/generator");
}

TEST(ProjectRoot, NoBuildCppIsAnError) {
    TempDir dir;
    std::filesystem::create_directories(dir / "a/b");

    const auto root = bb::find_project_root(dir / "a/b");
    ASSERT_FALSE(root);
    EXPECT_NE(root.error().find("bb init"), std::string::npos) << root.error();
}
