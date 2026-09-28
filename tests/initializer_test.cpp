#include "initializer.hpp"
#include "bb/build.hpp"
#include "compilation_database.hpp"
#include "compile_flags.hpp"
#include "process.hpp"
#include "test_support.hpp"

#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>

using bb::test::read_file;
using bb::test::TempDir;
using bb::test::write_file;

namespace {

std::expected<void, std::string> init_in(const std::filesystem::path& directory) {
    bb::test::ScopedCurrentPath current{directory};
    return bb::init();
}

bool has_trailing_whitespace(const std::string& text) {
    return std::regex_search(text, std::regex{"[ \t]+(\n|$)"});
}

} // namespace

TEST(Init, CreatesAProject) {
    TempDir dir;
    const auto result = init_in(dir.path());
    ASSERT_TRUE(result) << result.error();

    const auto build_file = read_file(dir / "build.cpp");
    EXPECT_NE(build_file.find(R"(b.executable({.name = "app", .sources = {"src/main.cpp"}});)"),
              std::string::npos)
        << build_file;

    const auto main_file = read_file(dir / "src/main.cpp");
    EXPECT_NE(main_file.find("Hello!"), std::string::npos) << main_file;

    for (const auto* file : {&build_file, &main_file}) {
        EXPECT_FALSE(has_trailing_whitespace(*file)) << *file;
        EXPECT_TRUE(file->ends_with('\n')) << *file;
    }

    EXPECT_EQ(read_file(dir / ".gitignore"), "/bb/\n/compile_commands.json\n");
    EXPECT_TRUE(std::filesystem::exists(dir / ".git"));
}

TEST(Init, WritesCompileCommandsMatchingWhatTheRunnerCompiles) {
    TempDir dir;
    ASSERT_TRUE(init_in(dir.path()));

    const auto database = nlohmann::json::parse(read_file(dir / "compile_commands.json"));
    ASSERT_EQ(database.size(), 2u);

    const auto build_file = bb::build_configuration_command(dir.path());
    EXPECT_EQ(database[0]["directory"], dir.path().string());
    EXPECT_EQ(database[0]["file"], "build.cpp");
    EXPECT_EQ(database[0]["arguments"], nlohmann::json(build_file.arguments));

    const auto main_file = bb::source_command(
        dir.path(), bb::target_flags(bb::Build{}.cpp_standard(), {}), "src/main.cpp");
    EXPECT_EQ(database[1]["file"], "src/main.cpp");
    EXPECT_EQ(database[1]["arguments"], nlohmann::json(main_file.arguments));
}

TEST(Init, AddsItsEntriesToAnExistingGitignore) {
    TempDir dir;
    write_file(dir / ".gitignore", "*.o\n/bb/"); // no final newline, one entry already there

    const auto result = init_in(dir.path());
    ASSERT_TRUE(result) << result.error();
    EXPECT_EQ(read_file(dir / ".gitignore"), "*.o\n/bb/\n/compile_commands.json\n");
}

TEST(Init, RefusesToOverwriteAProject) {
    TempDir dir;
    write_file(dir / "build.cpp", "existing");
    write_file(dir / ".gitignore", "*.o\n");

    const auto result = init_in(dir.path());
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("already exists"), std::string::npos) << result.error();

    EXPECT_EQ(read_file(dir / "build.cpp"), "existing");
    EXPECT_EQ(read_file(dir / ".gitignore"), "*.o\n");
    EXPECT_FALSE(std::filesystem::exists(dir / "src"));
    EXPECT_FALSE(std::filesystem::exists(dir / "compile_commands.json"));
}

TEST(Init, DoesNotCreateANestedGitRepository) {
    TempDir dir;
    const auto git = bb::run_process_capture({"git", "-C", dir.path().string(), "init", "-q"});
    ASSERT_TRUE(git && git->exit_code == 0);
    std::filesystem::create_directories(dir / "tools");

    const auto result = init_in(dir / "tools");
    ASSERT_TRUE(result) << result.error();
    EXPECT_TRUE(std::filesystem::exists(dir / "tools/build.cpp"));
    EXPECT_FALSE(std::filesystem::exists(dir / "tools/.git"));
}
