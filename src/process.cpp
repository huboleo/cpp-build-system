#include "process.hpp"
#include <cerrno>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

std::expected<int, std::string> bb::run_process(std::vector<std::string> arguments) {
    if (arguments.empty() || arguments.front().empty()) {
        return std::unexpected("Process requires a program name");
    }

    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid == -1) {
        return std::unexpected("Could not start " + arguments.front() + ": " +
                               std::error_code(errno, std::generic_category()).message());
    }

    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127);
    }

    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        return std::unexpected("Could not wait for " + arguments.front() + ": " +
                               std::error_code(errno, std::generic_category()).message());
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return std::unexpected(arguments.front() + " did not exit normally");
}
