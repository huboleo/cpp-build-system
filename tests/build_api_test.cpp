#include "bb/build.hpp"
#include "build_access.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <vector>

using bb::detail::BuildAccess;
using Strings = std::vector<std::string>;

// Handles can be copied, but only Build can create them, and an executable can't stand in for a
// library.
static_assert(std::is_copy_constructible_v<bb::Library>);
static_assert(std::is_copy_assignable_v<bb::Library>);
static_assert(!std::is_default_constructible_v<bb::Library>);
static_assert(!std::is_default_constructible_v<bb::Executable>);
static_assert(!std::is_constructible_v<bb::Library, std::uint32_t>);
static_assert(!std::is_constructible_v<bb::Executable, std::uint32_t>);
static_assert(!std::is_constructible_v<bb::Library, bb::Executable>);

namespace {

Strings errors_of(const std::function<void(bb::Build&)>& describe) {
    bb::Build b;
    describe(b);
    return BuildAccess::errors(b);
}

} // namespace

TEST(Build, DefaultsToCpp23) {
    bb::Build b;
    EXPECT_EQ(b.cpp_standard(), bb::CppStandard::CPP_23);

    b.set_cpp_standard(bb::CppStandard::CPP_17);
    EXPECT_EQ(b.cpp_standard(), bb::CppStandard::CPP_17);
}

TEST(Build, RecordsTargets) {
    bb::Build b;
    b.executable({.name = "app", .sources = {"src/main.cpp"}, .include_paths = {"include"}});
    b.library({.name = "core", .sources = {"src/core.cpp"}, .public_include_paths = {"include"}});

    const auto& executables = BuildAccess::executables(b);
    ASSERT_EQ(executables.size(), 1u);
    EXPECT_EQ(executables[0].name, "app");
    EXPECT_EQ(executables[0].sources, Strings{"src/main.cpp"});
    EXPECT_EQ(executables[0].include_paths, Strings{"include"});

    const auto& libraries = BuildAccess::libraries(b);
    ASSERT_EQ(libraries.size(), 1u);
    EXPECT_EQ(libraries[0].name, "core");
    EXPECT_EQ(libraries[0].public_include_paths, Strings{"include"});

    EXPECT_TRUE(BuildAccess::errors(b).empty());
}

TEST(Build, LinksResolveToTheirLibraries) {
    bb::Build b;
    const auto core = b.library({.name = "core"});
    const auto util = b.library({.name = "util"});
    b.executable({.name = "app", .links = {core, util}});

    const auto& links = BuildAccess::executables(b)[0].links;
    ASSERT_EQ(links.size(), 2u);
    EXPECT_EQ(BuildAccess::resolve(b, links[0]).name, "core");
    EXPECT_EQ(BuildAccess::resolve(b, links[1]).name, "util");
}

TEST(Build, HandlesStayValidAsTargetsAreAdded) {
    bb::Build b;
    const auto core = b.library({.name = "core"});
    for (int i = 0; i < 1000; ++i) {
        b.library({.name = "library-" + std::to_string(i)});
    }

    const auto copy = core;
    EXPECT_EQ(BuildAccess::resolve(b, core).name, "core");
    EXPECT_EQ(BuildAccess::resolve(b, copy).name, "core");
}

TEST(Build, RejectsDuplicateNamesAcrossKinds) {
    const auto errors = errors_of([](bb::Build& b) {
        b.executable({.name = "app"});
        b.executable({.name = "app"});
        b.library({.name = "app"});
    });

    EXPECT_EQ(errors, (Strings{
                          "executable 'app': name is already used by another target",
                          "library 'app': name is already used by another target",
                      }));
}

TEST(Build, RejectsMissingNames) {
    const auto errors = errors_of([](bb::Build& b) {
        b.executable({.name = "", .sources = {"src/tool.cpp"}});
        b.library({.name = ""});
    });

    EXPECT_EQ(errors, (Strings{
                          "executable with source 'src/tool.cpp' has no name",
                          "library has no name and no sources",
                      }));
}

TEST(Build, RejectsNamesThatAreNotPlainFileNames) {
    const auto errors = errors_of([](bb::Build& b) {
        b.executable({.name = "tools/app"});
        b.executable({.name = "win\\app"});
        b.library({.name = "."});
        b.library({.name = ".."});
    });

    EXPECT_EQ(errors, (Strings{
                          "executable 'tools/app': name must not contain '/' or '\\'",
                          "executable 'win\\app': name must not contain '/' or '\\'",
                          "library '.': name is not a valid file name",
                          "library '..': name is not a valid file name",
                      }));
}

namespace {

// Directory iteration order isn't specified, so the tests compare sorted lists.
Strings sorted_sources(bb::Build& b, const std::function<Strings(bb::Build&)>& find) {
    auto sources = find(b);
    std::ranges::sort(sources);
    return sources;
}

} // namespace

TEST(AllSourcesFrom, FindsCompiledFilesButNotHeaders) {
    bb::test::TempDir dir;
    for (const auto* file : {"src/main.cpp", "src/utils/strings.cc", "src/utils/strings.hpp",
                             "src/utils/detail.h", "src/notes.txt"}) {
        bb::test::write_file(dir / file, "");
    }

    bb::Build b;
    const auto sources =
        sorted_sources(b, [&](bb::Build& build) { return build.all_sources_from(dir / "src"); });
    EXPECT_EQ(sources, (Strings{(dir / "src/main.cpp").string(),
                                (dir / "src/utils/strings.cc").string()}));
    EXPECT_TRUE(BuildAccess::errors(b).empty());
}

TEST(AllSourcesFrom, UsesTheGivenExtensions) {
    bb::test::TempDir dir;
    for (const auto* file : {"src/main.cpp", "src/kernel.cu", "src/shader.metal"}) {
        bb::test::write_file(dir / file, "");
    }

    bb::Build b;
    const auto sources = sorted_sources(b, [&](bb::Build& build) {
        return build.all_sources_from(dir / "src", {".cu", ".metal"});
    });
    EXPECT_EQ(sources, (Strings{(dir / "src/kernel.cu").string(),
                                (dir / "src/shader.metal").string()}));
}

TEST(AllSourcesFrom, ReportsAMissingDirectory) {
    bb::test::TempDir dir;
    bb::Build b;

    EXPECT_TRUE(b.all_sources_from(dir / "srcc").empty());
    ASSERT_EQ(BuildAccess::errors(b).size(), 1u);
    EXPECT_TRUE(BuildAccess::errors(b)[0].starts_with("cannot list sources in"))
        << BuildAccess::errors(b)[0];
}

TEST(Build, AcceptsOrdinaryNames) {
    EXPECT_TRUE(errors_of([](bb::Build& b) {
                    b.executable({.name = "my-app.v2_1"});
                    b.library({.name = "core"});
                }).empty());
}
