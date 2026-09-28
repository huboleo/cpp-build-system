#include "fingerprint.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <functional>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

namespace {

bb::FingerprintInputs sample_inputs() {
    return {
        .compiler =
            {
                .path = "/usr/bin/clang++",
                .resolved_path = "/usr/bin/clang++",
                .version = "clang version 21",
                .size = 1234,
                .modification_time = 5678,
            },
        .command = {"/usr/bin/clang++", "-std=c++23", "build.cpp"},
        .environment = {{"SDKROOT", "/sdk"}},
        .files = {{"build.cpp", "hash-a"}, {"build.hpp", "hash-b"}},
    };
}

} // namespace

TEST(Fingerprint, IsDeterministic) {
    EXPECT_EQ(bb::compute_fingerprint(sample_inputs()), bb::compute_fingerprint(sample_inputs()));
}

TEST(Fingerprint, ChangesWhenAnyInputChanges) {
    using Change = std::function<void(bb::FingerprintInputs&)>;
    const std::vector<std::pair<std::string, Change>> changes{
        {"compiler path", [](auto& inputs) { inputs.compiler.path = "/opt/llvm/bin/clang++"; }},
        {"resolved compiler", [](auto& inputs) { inputs.compiler.resolved_path = "/opt/clang-23"; }},
        {"compiler version", [](auto& inputs) { inputs.compiler.version = "clang version 23"; }},
        {"compiler size", [](auto& inputs) { inputs.compiler.size += 1; }},
        {"compiler modification time", [](auto& inputs) { inputs.compiler.modification_time += 1; }},
        {"command", [](auto& inputs) { inputs.command.push_back("-O2"); }},
        {"environment", [](auto& inputs) { inputs.environment["CPATH"] = "/include"; }},
        {"file contents", [](auto& inputs) { inputs.files["build.cpp"] = "hash-c"; }},
        {"file list", [](auto& inputs) { inputs.files["extra.hpp"] = "hash-d"; }},
    };

    const auto original = bb::compute_fingerprint(sample_inputs());
    for (const auto& [what, change] : changes) {
        auto inputs = sample_inputs();
        change(inputs);
        EXPECT_NE(bb::compute_fingerprint(inputs), original) << what;
    }
}

TEST(Fingerprint, KeepsFieldBoundaries) {
    // Every field is length-prefixed, so moving characters between neighbours changes the hash.
    auto first = sample_inputs();
    auto second = sample_inputs();
    first.command = {"ab", "c"};
    second.command = {"a", "bc"};
    EXPECT_NE(bb::compute_fingerprint(first), bb::compute_fingerprint(second));

    first = sample_inputs();
    second = sample_inputs();
    first.environment = {{"A", "BC"}};
    second.environment = {{"AB", "C"}};
    EXPECT_NE(bb::compute_fingerprint(first), bb::compute_fingerprint(second));
}

TEST(FingerprintRecord, RoundTrips) {
    bb::test::TempDir dir;
    const auto inputs = sample_inputs();

    const auto written = bb::write_fingerprint_record(dir / "record.json", inputs, "runner-hash");
    ASSERT_TRUE(written) << written.error();

    const auto record = bb::read_fingerprint_record(dir / "record.json");
    ASSERT_TRUE(record);
    EXPECT_EQ(record->fingerprint, bb::compute_fingerprint(inputs));
    EXPECT_EQ(record->runner_hash, "runner-hash");
    EXPECT_EQ(record->files, inputs.files);
    EXPECT_FALSE(std::filesystem::exists(dir / "record.json.tmp"));
}

TEST(FingerprintRecord, UnusableRecordsMeanNoCache) {
    bb::test::TempDir dir;
    EXPECT_FALSE(bb::read_fingerprint_record(dir / "missing.json"));

    const nlohmann::json valid{
        {"fingerprint_version", std::string{bb::FINGERPRINT_VERSION}},
        {"fingerprint", "f"},
        {"runner_hash", "r"},
        {"files", {{"build.cpp", "hash"}}},
    };
    bb::test::write_file(dir / "valid.json", valid.dump());
    ASSERT_TRUE(bb::read_fingerprint_record(dir / "valid.json"));

    using Change = std::function<void(nlohmann::json&)>;
    const std::vector<std::pair<std::string, Change>> changes{
        {"older version", [](auto& record) { record["fingerprint_version"] = "bb-fingerprint-v0"; }},
        {"no fingerprint", [](auto& record) { record.erase("fingerprint"); }},
        {"no runner hash", [](auto& record) { record.erase("runner_hash"); }},
        {"files not an object", [](auto& record) { record["files"] = nlohmann::json::array(); }},
        {"file hash not a string", [](auto& record) { record["files"]["build.cpp"] = 1; }},
    };

    for (const auto& [what, change] : changes) {
        auto record = valid;
        change(record);
        bb::test::write_file(dir / "record.json", record.dump());
        EXPECT_FALSE(bb::read_fingerprint_record(dir / "record.json")) << what;
    }

    for (const auto& contents : {"{not json", "[]"}) {
        bb::test::write_file(dir / "record.json", contents);
        EXPECT_FALSE(bb::read_fingerprint_record(dir / "record.json")) << contents;
    }
}
