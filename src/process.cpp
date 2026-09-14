#include "process.hpp"
#include <array>
#include <cerrno>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace {

std::expected<int, std::string> wait_for_process(pid_t pid, const std::string& program) {
    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        return std::unexpected("Could not wait for " + program + ": " +
                               std::error_code(errno, std::generic_category()).message());
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return std::unexpected(program + " did not exit normally");
}

std::vector<char*> make_argv(std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    return argv;
}

} // namespace

std::expected<int, std::string> bb::run_process(std::vector<std::string> arguments) {
    if (arguments.empty() || arguments.front().empty()) {
        return std::unexpected("Process requires a program name");
    }

    auto argv = make_argv(arguments);

    const pid_t pid = fork();
    if (pid == -1) {
        return std::unexpected("Could not start " + arguments.front() + ": " +
                               std::error_code(errno, std::generic_category()).message());
    }

    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127);
    }

    return wait_for_process(pid, arguments.front());
}

std::expected<bb::ProcessOutput, std::string>
bb::run_process_capture(std::vector<std::string> arguments) {
    if (arguments.empty() || arguments.front().empty()) {
        return std::unexpected("Process requires a program name");
    }

    int descriptors[2];
    if (pipe(descriptors) == -1) {
        return std::unexpected("Could not create process output pipe: " +
                               std::error_code(errno, std::generic_category()).message());
    }

    auto argv = make_argv(arguments);

    const pid_t pid = fork();
    if (pid == -1) {
        const auto error = std::error_code(errno, std::generic_category()).message();
        close(descriptors[0]);
        close(descriptors[1]);
        return std::unexpected("Could not start " + arguments.front() + ": " + error);
    }

    if (pid == 0) {
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) == -1 ||
            dup2(descriptors[1], STDERR_FILENO) == -1) {
            _exit(127);
        }
        close(descriptors[1]);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(descriptors[1]);

    std::string output;
    std::array<char, 4096> buffer;
    std::string read_error;

    while (true) {
        const ssize_t count = read(descriptors[0], buffer.data(), buffer.size());
        if (count > 0) {
            output.append(buffer.data(), static_cast<std::size_t>(count));
            continue;
        }

        if (count == 0) {
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        read_error = std::error_code(errno, std::generic_category()).message();
        break;
    }

    close(descriptors[0]);

    auto exit_code = wait_for_process(pid, arguments.front());
    if (!exit_code) {
        return std::unexpected(exit_code.error());
    }

    if (!read_error.empty()) {
        return std::unexpected("Could not read output from " + arguments.front() + ": " +
                               read_error);
    }

    return ProcessOutput{
        .exit_code = *exit_code,
        .output = std::move(output),
    };
}
