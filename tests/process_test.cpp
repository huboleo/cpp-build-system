#include "process.hpp"

#include <csignal>
#include <cstdio>
#include <gtest/gtest.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

TEST(RunProcess, ReturnsTheExitCode) {
    for (const int code : {0, 1, 7}) {
        const auto result = bb::run_process({"/bin/sh", "-c", "exit " + std::to_string(code)});
        ASSERT_TRUE(result) << result.error();
        EXPECT_EQ(*result, code);
    }
}

TEST(RunProcess, ReportsASignalAs128PlusItsNumber) {
    const auto result = bb::run_process({"/bin/sh", "-c", "kill -TERM $$"});
    ASSERT_TRUE(result) << result.error();
    EXPECT_EQ(*result, 128 + SIGTERM);
}

TEST(RunProcess, Returns127WhenTheProgramCannotStart) {
    const auto result = bb::run_process({"/nonexistent/program"});
    ASSERT_TRUE(result) << result.error();
    EXPECT_EQ(*result, 127);
}

TEST(RunProcess, RejectsAnEmptyCommand) {
    EXPECT_FALSE(bb::run_process({}));
    EXPECT_FALSE(bb::run_process({""}));
    EXPECT_FALSE(bb::run_process_capture({}));
}

TEST(RunProcessCapture, CapturesStandardOutputAndError) {
    const auto result = bb::run_process_capture({"/bin/sh", "-c", "echo out; echo err >&2; exit 3"});
    ASSERT_TRUE(result) << result.error();
    EXPECT_EQ(result->exit_code, 3);
    EXPECT_EQ(result->output, "out\nerr\n");
}

TEST(ReplaceProcess, BecomesTheProgram) {
    // Flush first, so the child doesn't write this process's buffered output a second time.
    std::fflush(nullptr);
    const pid_t pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0) {
        (void)bb::replace_process({"/bin/sh", "-c", "exit 5"});
        _exit(99); // reached only if exec failed
    }

    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 5);
}

TEST(ReplaceProcess, ReturnsAnErrorWhenTheProgramCannotStart) {
    const auto error = bb::replace_process({"/nonexistent/program"});
    EXPECT_NE(error.find("/nonexistent/program"), std::string::npos) << error;
}
