#include "compilation_database.hpp"
#include "compile_flags.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using bb::test::TempDir;
using Arguments = std::vector<std::string>;

TEST(CompilationDatabase, SourceCommandCompilesOneFile) {
    const auto command = bb::source_command("/project", {"-std=c++20", "-I", "include"}, "src/main.cpp");

    EXPECT_EQ(command.directory, "/project");
    EXPECT_EQ(command.file, "src/main.cpp");
    EXPECT_EQ(command.arguments,
              (Arguments{"clang++", "-std=c++20", "-I", "include", "-c", "src/main.cpp"}));
}

TEST(CompilationDatabase, BuildConfigurationUsesBuildFileFlags) {
    Arguments expected{"clang++"};
    const auto flags = bb::build_file_flags();
    expected.insert(expected.end(), flags.begin(), flags.end());
    expected.insert(expected.end(), {"-c", "build.cpp"});

    const auto command = bb::build_configuration_command("/project");
    EXPECT_EQ(command.file, "build.cpp");
    EXPECT_EQ(command.arguments, expected);
}

TEST(CompilationDatabase, WritesEntries) {
    TempDir dir;
    const std::vector<bb::CompileCommand> commands{
        bb::source_command(dir.path(), {"-std=c++23"}, "a.cpp"),
    };

    const auto written = bb::write_compilation_database(dir / "compile_commands.json", commands,
                                                        bb::DatabaseWriteMode::CREATE);
    ASSERT_TRUE(written) << written.error();

    const auto database = nlohmann::json::parse(bb::test::read_file(dir / "compile_commands.json"));
    ASSERT_EQ(database.size(), 1u);
    EXPECT_EQ(database[0]["directory"], dir.path().string());
    EXPECT_EQ(database[0]["file"], "a.cpp");
    EXPECT_EQ(database[0]["arguments"], nlohmann::json(commands[0].arguments));
}

TEST(CompilationDatabase, CreateRefusesToOverwrite) {
    TempDir dir;
    bb::test::write_file(dir / "compile_commands.json", "keep me");

    const auto written =
        bb::write_compilation_database(dir / "compile_commands.json", {}, bb::DatabaseWriteMode::CREATE);
    EXPECT_FALSE(written);
    EXPECT_EQ(bb::test::read_file(dir / "compile_commands.json"), "keep me");
}

TEST(CompilationDatabase, ReplaceRewritesOnlyChangedContents) {
    TempDir dir;
    const auto path = dir / "compile_commands.json";
    std::vector<bb::CompileCommand> commands{bb::source_command(dir.path(), {"-std=c++23"}, "a.cpp")};

    ASSERT_TRUE(bb::write_compilation_database(path, commands, bb::DatabaseWriteMode::REPLACE));
    const auto original = bb::test::inode_of(path);

    ASSERT_TRUE(bb::write_compilation_database(path, commands, bb::DatabaseWriteMode::REPLACE));
    EXPECT_EQ(bb::test::inode_of(path), original) << "identical contents must not be rewritten";

    commands.push_back(bb::source_command(dir.path(), {"-std=c++23"}, "b.cpp"));
    ASSERT_TRUE(bb::write_compilation_database(path, commands, bb::DatabaseWriteMode::REPLACE));
    EXPECT_NE(bb::test::inode_of(path), original);
    EXPECT_EQ(nlohmann::json::parse(bb::test::read_file(path)).size(), 2u);
    EXPECT_FALSE(std::filesystem::exists(dir / "compile_commands.json.tmp"));
}
