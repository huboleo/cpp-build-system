#include "clang_tools.hpp"
#include "test_support.hpp"

#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using bb::test::TempDir;
using bb::test::write_file;
using Paths = std::vector<std::filesystem::path>;

TEST(FindClangTool, PrefersPath) {
    TempDir dir;
    bb::test::write_executable(dir / "clang-format", "#!/bin/sh\n");
    const char* path = std::getenv("PATH");
    bb::test::ScopedEnvironment environment{
        "PATH", dir.path().string() + ":" + (path != nullptr ? path : "")};

    const auto tool = bb::find_clang_tool("clang-format");
    ASSERT_TRUE(tool) << tool.error();
    EXPECT_EQ(*tool, dir / "clang-format");
}

TEST(FindClangTool, ExplainsHowToInstallAMissingTool) {
    const auto tool = bb::find_clang_tool("bb-test-no-such-tool");
    ASSERT_FALSE(tool);
    EXPECT_NE(tool.error().find("install LLVM"), std::string::npos) << tool.error();
}

TEST(FormattableFiles, FindsSourcesAndSkipsOutputAndHiddenDirectories) {
    TempDir dir;
    for (const auto* file : {
             "build.cpp", "src/main.cpp", "src/util/strings.cc", "src/c_code.c", "src/header.h",
             "include/app/app.hpp", "tools/bb/kept.cpp", // only the top-level bb/ is bb's output
             "bb/cache/generated.cpp", ".git/hooks/hook.cpp", "src/.hidden/skipped.cpp",
             "docs/readme.md", "src/notes.txt",
         }) {
        write_file(dir / file, "");
    }
    std::filesystem::create_symlink(dir / "missing.cpp", dir / "src/broken.cpp");

    const auto files = bb::formattable_files(dir.path());
    ASSERT_TRUE(files) << files.error();
    EXPECT_EQ(*files, (Paths{
                          "build.cpp",
                          "include/app/app.hpp",
                          "src/c_code.c",
                          "src/header.h",
                          "src/main.cpp",
                          "src/util/strings.cc",
                          "tools/bb/kept.cpp",
                      }));
}

TEST(LintableFiles, ListsTheCompilationDatabaseEntries) {
    TempDir dir;
    const auto project = dir.path().string();
    const nlohmann::json database{
        {{"directory", project}, {"file", "src/main.cpp"}, {"arguments", {"clang++"}}},
        {{"directory", project}, {"file", "build.cpp"}, {"arguments", {"clang++"}}},
        {{"directory", project + "/src"}, {"file", "../src/main.cpp"}, {"arguments", {"clang++"}}},
        {{"directory", "/elsewhere"}, {"file", project + "/lib/a.cpp"}, {"arguments", {"clang++"}}},
    };
    write_file(dir / "compile_commands.json", database.dump());

    const auto files = bb::lintable_files(dir.path());
    ASSERT_TRUE(files) << files.error();
    EXPECT_EQ(*files, (Paths{"build.cpp", "lib/a.cpp", "src/main.cpp"}));
}

TEST(LintableFiles, MissingDatabaseSuggestsBuilding) {
    TempDir dir;
    const auto files = bb::lintable_files(dir.path());
    ASSERT_FALSE(files);
    EXPECT_NE(files.error().find("run bb build"), std::string::npos) << files.error();
}

TEST(LintableFiles, RejectsInvalidDatabases) {
    TempDir dir;
    for (const auto* contents : {"not json", "{}", R"([{"directory": "/p", "file": 1}])"}) {
        write_file(dir / "compile_commands.json", contents);
        EXPECT_FALSE(bb::lintable_files(dir.path())) << contents;
    }
}
