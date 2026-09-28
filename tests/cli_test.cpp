// End-to-end tests: they run the real bb binary on throwaway projects, and compile real code, so
// each one takes about a second.

#include "clang_tools.hpp"
#include "process.hpp"
#include "test_support.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

using bb::test::TempDir;

namespace {

// Runs bb in directory and returns its exit code and everything it printed.
bb::ProcessOutput run_bb(const std::filesystem::path& directory,
                         std::vector<std::string> arguments) {
    bb::test::ScopedCurrentPath current{directory};
    arguments.insert(arguments.begin(), BB_EXECUTABLE);

    auto result = bb::run_process_capture(std::move(arguments));
    if (!result) {
        ADD_FAILURE() << result.error();
        return {.exit_code = -1, .output = {}};
    }
    return *result;
}

bool contains(const std::string& text, std::string_view part) {
    return text.find(part) != std::string::npos;
}

// Prints its arguments as [a][b], and exits with 7 when the first one is "fail".
constexpr std::string_view ARGUMENTS_PROGRAM = R"(#include <cstdio>
#include <string_view>

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        std::printf("[%s]", argv[i]);
    }
    return argc > 1 && std::string_view{argv[1]} == "fail" ? 7 : 0;
}
)";

// A project created with bb init.
class BbProject : public testing::Test {
  protected:
    void SetUp() override {
        const auto init = run_bb(project.path(), {"init"});
        ASSERT_EQ(init.exit_code, 0) << init.output;
    }

    bb::ProcessOutput run(std::vector<std::string> arguments,
                          const std::filesystem::path& subdirectory = {}) {
        return run_bb(project.path() / subdirectory, std::move(arguments));
    }

    void write(const std::filesystem::path& relative, std::string_view contents) {
        bb::test::write_file(project / relative, contents);
    }

    TempDir project;
};

} // namespace

TEST(BbCommandLine, NoArgumentsPrintsUsage) {
    TempDir dir;
    const auto result = run_bb(dir.path(), {});
    EXPECT_EQ(result.exit_code, 1);
    EXPECT_TRUE(contains(result.output, "usage: bb <command>")) << result.output;
}

TEST(BbCommandLine, HelpPrintsUsage) {
    TempDir dir;
    const auto result = run_bb(dir.path(), {"help"});
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(contains(result.output, "usage: bb <command>")) << result.output;
}

TEST(BbCommandLine, RejectsUnknownCommandsAndArguments) {
    TempDir dir;
    const std::vector<std::pair<std::vector<std::string>, std::string>> cases{
        {{"frobnicate"}, "error: unknown command 'frobnicate'"},
        {{"build", "extra"}, "error: unexpected argument 'extra' for build"},
        {{"run", "extra"}, "error: unexpected argument 'extra' for run; pass program arguments after --"},
        {{"init", "--existing"}, "error: unexpected argument '--existing' for init"},
        {{"fmt", "extra"}, "error: unexpected argument 'extra' for fmt"},
        {{"lint", "--check"}, "error: unexpected argument '--check' for lint"},
    };

    for (const auto& [arguments, message] : cases) {
        const auto result = run_bb(dir.path(), arguments);
        EXPECT_EQ(result.exit_code, 1) << arguments[0];
        EXPECT_TRUE(contains(result.output, message)) << result.output;
    }
}

TEST(BbCommandLine, BuildOutsideAProjectFails) {
    TempDir dir;
    const auto result = run_bb(dir.path(), {"build"});
    EXPECT_EQ(result.exit_code, 1);
    EXPECT_TRUE(contains(result.output, "Cannot find build.cpp")) << result.output;
    EXPECT_FALSE(std::filesystem::exists(dir / "bb"));
}

TEST_F(BbProject, NewProjectBuildsAndRuns) {
    const auto first = run({"run"});
    EXPECT_EQ(first.exit_code, 0) << first.output;
    EXPECT_TRUE(contains(first.output, "build runner: out of date, compiling")) << first.output;
    EXPECT_TRUE(contains(first.output, "Hello!")) << first.output;

    const auto second = run({"build"});
    EXPECT_EQ(second.exit_code, 0) << second.output;
    EXPECT_TRUE(contains(second.output, "build runner: cached")) << second.output;
}

TEST_F(BbProject, RunPassesArgumentsAndExitCode) {
    write("src/main.cpp", ARGUMENTS_PROGRAM);

    const auto passed = run({"run", "--", "a", "b c"});
    EXPECT_EQ(passed.exit_code, 0) << passed.output;
    EXPECT_TRUE(contains(passed.output, "[a][b c]")) << passed.output;

    const auto failed = run({"run", "--", "fail"});
    EXPECT_EQ(failed.exit_code, 7) << failed.output;
}

TEST_F(BbProject, WorksFromASubdirectory) {
    write("src/main.cpp", ARGUMENTS_PROGRAM);
    std::filesystem::create_directories(project / "src/nested");

    const auto result = run({"run", "--", "x"}, "src/nested");
    EXPECT_EQ(result.exit_code, 0) << result.output;
    EXPECT_TRUE(contains(result.output, "[x]")) << result.output;
    EXPECT_FALSE(std::filesystem::exists(project / "src/nested/bb"));
}

TEST_F(BbProject, ReportsErrorsFromBuildCpp) {
    write("build.cpp", R"(#include <bb/build.hpp>

void build(bb::Build& b)
{
    b.executable({.name = "app", .sources = {"src/main.cpp"}});
    b.executable({.name = "app", .sources = {"src/main.cpp"}});
}
)");

    const auto result = run({"build"});
    EXPECT_EQ(result.exit_code, 1);
    EXPECT_TRUE(contains(result.output, "error: executable 'app': name is already used by another target"))
        << result.output;
}

TEST_F(BbProject, CompileErrorsFailTheBuild) {
    write("src/main.cpp", "int main() { return missing; }\n");

    const auto result = run({"build"});
    EXPECT_NE(result.exit_code, 0);
    EXPECT_TRUE(contains(result.output, "undeclared identifier 'missing'")) << result.output;
}

TEST_F(BbProject, WarnsAboutBuildCppButNotAboutBbHeaders) {
    write("build.cpp", R"(#include <bb/build.hpp>

void build(bb::Build& b)
{
    int unused = 0;
    b.executable({.name = "app", .sources = {"src/main.cpp"}});
}
)");

    const auto result = run({"build"});
    EXPECT_EQ(result.exit_code, 0) << result.output;
    EXPECT_TRUE(contains(result.output, "unused variable 'unused'")) << result.output;
    EXPECT_FALSE(contains(result.output, "build.hpp")) << result.output;
}

TEST_F(BbProject, CompileCommandsFollowBuildCppAndAreOnlyRewrittenWhenChanged) {
    write("build.cpp", R"(#include <bb/build.hpp>

void build(bb::Build& b)
{
    b.set_cpp_standard(bb::CppStandard::CPP_20);
    b.executable({.name = "app", .sources = {"src/main.cpp"}, .include_paths = {"include"}});
}
)");
    write("src/main.cpp", "int main() {}\n");

    const auto first = run({"build"});
    ASSERT_EQ(first.exit_code, 0) << first.output;

    const auto path = project / "compile_commands.json";
    const auto database = nlohmann::json::parse(bb::test::read_file(path));
    ASSERT_EQ(database.size(), 2u);
    EXPECT_EQ(database[1]["arguments"],
              (nlohmann::json{"clang++", "-std=c++20", "-I", "include", "-c", "src/main.cpp"}));

    const auto inode = bb::test::inode_of(path);
    const auto second = run({"build"});
    ASSERT_EQ(second.exit_code, 0) << second.output;
    EXPECT_EQ(bb::test::inode_of(path), inode);
}

TEST_F(BbProject, FmtChecksAndFixesFormatting) {
    if (const auto tool = bb::find_clang_tool("clang-format"); !tool) {
        GTEST_SKIP() << tool.error();
    }

    const auto clean = run({"fmt", "--check"});
    EXPECT_EQ(clean.exit_code, 0) << clean.output;

    const std::string messy = "int main(){return 0;}\n";
    write("src/main.cpp", messy);

    const auto check = run({"fmt", "--check"});
    EXPECT_NE(check.exit_code, 0) << check.output;
    EXPECT_TRUE(contains(check.output, "src/main.cpp")) << check.output;
    EXPECT_EQ(bb::test::read_file(project / "src/main.cpp"), messy);

    const auto format = run({"fmt"}, "src");
    EXPECT_EQ(format.exit_code, 0) << format.output;
    EXPECT_EQ(bb::test::read_file(project / "src/main.cpp"), "int main() {\n    return 0;\n}\n");
    EXPECT_EQ(run({"fmt", "--check"}).exit_code, 0);
}

TEST_F(BbProject, LintIsCleanOnANewProjectAndReportsProblems) {
    if (const auto tool = bb::find_clang_tool("clang-tidy"); !tool) {
        GTEST_SKIP() << tool.error();
    }

    const auto clean = run({"lint"});
    EXPECT_EQ(clean.exit_code, 0) << clean.output;
    EXPECT_FALSE(contains(clean.output, "warning:")) << clean.output;

    write("src/main.cpp", R"(int main() {
    int* pointer = 0;
    return pointer == 0 ? 0 : 1;
}
)");
    const auto dirty = run({"lint"});
    EXPECT_TRUE(contains(dirty.output, "[modernize-use-nullptr]")) << dirty.output;
}

TEST_F(BbProject, BuildDoesNotWaitForARunningProgram) {
    write("src/main.cpp", R"(#include <chrono>
#include <thread>

int main()
{
    std::this_thread::sleep_for(std::chrono::seconds(30));
}
)");
    const auto built = run({"build"});
    ASSERT_EQ(built.exit_code, 0) << built.output;

    // Start `bb run` in the background. Its program keeps running for 30 seconds.
    std::fflush(nullptr);
    const pid_t running = fork();
    ASSERT_NE(running, -1);
    if (running == 0) {
        const int null = open("/dev/null", O_WRONLY);
        if (null != -1 && chdir(project.path().c_str()) == 0) {
            dup2(null, STDOUT_FILENO);
            dup2(null, STDERR_FILENO);
            execl(BB_EXECUTABLE, BB_EXECUTABLE, "run", nullptr);
        }
        _exit(127);
    }

    // The runner is cached, so the program is running well within this time.
    std::this_thread::sleep_for(std::chrono::seconds(2));
    const auto build = run({"build"});

    int status = 0;
    const bool still_running = waitpid(running, &status, WNOHANG) == 0;
    // bb replaced itself with the runner and then the program, so this pid is the program.
    kill(running, SIGTERM);
    waitpid(running, &status, 0);

    EXPECT_EQ(build.exit_code, 0) << build.output;
    EXPECT_TRUE(still_running) << "bb build should finish while the program is still running";
}
