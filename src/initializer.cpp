#include "initializer.hpp"
#include "process.hpp"
#include <expected>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

std::string json_string(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";

    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            result += '\\';
            result += static_cast<char>(c);
        } else if (c < 0x20) {
            result += "\\u00";
            result += hex[c >> 4];
            result += hex[c & 0x0f];
        } else {
            result += static_cast<char>(c);
        }
    }

    result += '"';
    return result;
}

std::expected<void, std::string> write_main_file(const std::filesystem::path& main_file) {
    std::ofstream main_output{main_file};

    if (!main_output) {
        return std::unexpected("Could not open " + main_file.string());
    }

    main_output << R"(#include <print>

int main()
{
    std::println("Hello!");
    return 0;
}
    )";

    main_output.close();

    if (!main_output) {
        return std::unexpected("Could not write " + main_file.string());
    }

    return {};
}

std::expected<void, std::string> write_build_file(const std::filesystem::path& build_file) {
    std::ofstream build_output{build_file};

    if (!build_output) {
        return std::unexpected("Could not open " + build_file.string());
    }

    build_output << R"(#include <bb/build.hpp>

void build(bb::Build& b)
{
    b.executable("app", {"src/main.cpp"});
}
    )";

    build_output.close();

    if (!build_output) {
        return std::unexpected("Could not write " + build_file.string());
    }

    return {};
}

std::expected<void, std::string> write_compile_commands(const std::filesystem::path& project_dir) {
    const auto commands_file = project_dir / "compile_commands.json";

    // Fixed API location for the development prototype.
    const std::filesystem::path bb_include_dir = "/Users/hubert/Projects/cpp-build-system/include";
    std::ofstream commands_output{commands_file};

    if (!commands_output) {
        return std::unexpected("Could not open " + commands_file.string());
    }

    const auto directory = json_string(project_dir.string());
    const auto include_dir = json_string(bb_include_dir.string());

    commands_output << R"([
  {
    "directory": )" << directory
                    << R"(,
    "file": "build.cpp",
    "arguments": [
      "clang++",
      "-std=c++23",
      "-I",
      )" << include_dir
                    << R"(,
      "-c",
      "build.cpp"
    ]
  },
  {
    "directory": )" << directory
                    << R"(,
    "file": "src/main.cpp",
    "arguments": [
      "clang++",
      "-std=c++23",
      "-c",
      "src/main.cpp"
    ]
  }
]
)";

    commands_output.close();

    if (!commands_output) {
        return std::unexpected("Could not write " + commands_file.string());
    }

    return {};
}

} // namespace

std::expected<void, std::string> bb::init() {
    namespace fs = std::filesystem;

    std::error_code error;
    const auto project_dir = fs::current_path(error);

    if (error) {
        return std::unexpected("Could not determine project directory: " + error.message());
    }

    const auto build_file = project_dir / "build.cpp";
    const auto source_dir = project_dir / "src";
    const auto main_file = source_dir / "main.cpp";
    const auto commands_file = project_dir / "compile_commands.json";

    for (const auto& path : {build_file, main_file, commands_file}) {
        const bool exists = fs::exists(path, error);

        if (error) {
            return std::unexpected("Could not inspect " + path.string() + ": " + error.message());
        }

        if (exists) {
            return std::unexpected("File already exists: " + path.string());
        }
    }

    fs::create_directories(source_dir, error);

    if (error) {
        return std::unexpected("Could not create source directory: " + error.message());
    }

    if (auto result = write_main_file(main_file); !result) {
        return std::unexpected(result.error());
    }

    if (auto result = write_build_file(build_file); !result) {
        return std::unexpected(result.error());
    }

    if (auto result = write_compile_commands(project_dir); !result) {
        return std::unexpected(result.error());
    }

    auto result = run_process({"git", "-C", project_dir.string(), "init"});
    if (!result) {
        return std::unexpected("Project files were created, but Git initialization failed: " +
                               result.error());
    }

    if (*result != 0) {
        return std::unexpected(
            "Project files were created, but git init failed with exit code " +
            std::to_string(*result) +
            (*result == 127 ? " (Git may be unavailable in PATH)" : ""));
    }

    return {};
}
