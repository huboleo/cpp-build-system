#include "compiler.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

// These tests need clang++ on PATH, like bb itself.

TEST(Compiler, FindsClangOnPath) {
    const auto compiler = bb::identify_compiler("clang++");
    ASSERT_TRUE(compiler) << compiler.error();

    EXPECT_TRUE(compiler->path.is_absolute());
    EXPECT_EQ(compiler->path.filename(), "clang++");
    EXPECT_EQ(compiler->resolved_path, std::filesystem::canonical(compiler->path));
    EXPECT_NE(compiler->version.find("clang"), std::string::npos) << compiler->version;
    EXPECT_GT(compiler->size, 0u);
}

// Clang picks C or C++ mode from the name it is invoked by, so bb must invoke the symlink, and
// only use the file it points to for the fingerprint.
TEST(Compiler, KeepsTheSymlinkButIdentifiesItsTarget) {
    const auto real = bb::identify_compiler("clang++");
    ASSERT_TRUE(real) << real.error();

    bb::test::TempDir dir;
    std::filesystem::create_symlink(real->resolved_path, dir / "clang++");

    const auto linked = bb::identify_compiler((dir / "clang++").string());
    ASSERT_TRUE(linked) << linked.error();
    EXPECT_EQ(linked->path, dir / "clang++");
    EXPECT_EQ(linked->resolved_path, real->resolved_path);
}

TEST(Compiler, ReportsMissingOrUnusableCompilers) {
    bb::test::TempDir dir;
    bb::test::write_file(dir / "not-executable", "");

    EXPECT_FALSE(bb::identify_compiler("bb-test-no-such-compiler"));
    EXPECT_FALSE(bb::identify_compiler((dir / "missing").string()));
    EXPECT_FALSE(bb::identify_compiler((dir / "not-executable").string()));
}
