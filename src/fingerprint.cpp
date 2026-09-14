#include "fingerprint.hpp"
#include "hashing.hpp"

#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace {

void append_u64(std::string& out, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) {
        out.push_back(static_cast<char>((value >> (i * 8)) & 0xFFu));
    }
}

void append_framed(std::string& out, std::string_view value) {
    append_u64(out, value.size());
    out.append(value);
}

} // namespace

std::string bb::compute_fingerprint(const FingerprintInputs& inputs) {
    std::string encoded;

    append_framed(encoded, FINGERPRINT_VERSION);

    append_framed(encoded, inputs.compiler.path.string());
    append_framed(encoded, inputs.compiler.version);
    append_u64(encoded, inputs.compiler.size);
    append_u64(encoded, static_cast<std::uint64_t>(inputs.compiler.modification_time));

    append_u64(encoded, inputs.command.size());
    for (const auto& argument : inputs.command) {
        append_framed(encoded, argument);
    }

    append_u64(encoded, inputs.environment.size());
    for (const auto& [name, value] : inputs.environment) {
        append_framed(encoded, name);
        append_framed(encoded, value);
    }

    append_u64(encoded, inputs.files.size());
    for (const auto& [path, hash] : inputs.files) {
        append_framed(encoded, path);
        append_framed(encoded, hash);
    }

    return hash_bytes(encoded);
}

std::expected<void, std::string>
bb::write_fingerprint_record(const std::filesystem::path& path,
                             const FingerprintInputs& inputs,
                             std::string_view runner_hash) {
    const nlohmann::json record{
        {"fingerprint_version", std::string{FINGERPRINT_VERSION}},
        {"fingerprint", compute_fingerprint(inputs)},
        {"runner_hash", runner_hash},
        {"compiler",
         {
             {"path", inputs.compiler.path.string()},
             {"version", inputs.compiler.version},
             {"size", inputs.compiler.size},
             {"modification_time", inputs.compiler.modification_time},
         }},
        {"command", inputs.command},
        {"environment", inputs.environment},
        {"files", inputs.files},
    };

    auto temporary_path = path;
    temporary_path += ".tmp";

    std::ofstream output{temporary_path, std::ios::binary | std::ios::trunc};
    if (!output) {
        return std::unexpected("Cannot open temporary fingerprint record: " +
                               temporary_path.string());
    }

    output << record.dump(2) << "\n";
    output.close();

    if (!output) {
        std::error_code ignored;
        std::filesystem::remove(temporary_path, ignored);
        return std::unexpected("Cannot write temporary fingerprint record: " +
                               temporary_path.string());
    }

    std::error_code error;
    std::filesystem::rename(temporary_path, path, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary_path, ignored);
        return std::unexpected("Cannot publish fingerprint record: " + error.message());
    }

    return {};
}

std::optional<bb::FingerprintRecord>
bb::read_fingerprint_record(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::nullopt;
    }

    const auto record = nlohmann::json::parse(input, nullptr, false);
    if (!record.is_object()) {
        return std::nullopt;
    }

    const auto version = record.find("fingerprint_version");
    if (version == record.end() || !version->is_string() ||
        version->get<std::string>() != FINGERPRINT_VERSION) {
        return std::nullopt;
    }

    const auto fingerprint = record.find("fingerprint");
    if (fingerprint == record.end() || !fingerprint->is_string()) {
        return std::nullopt;
    }

    const auto runner_hash = record.find("runner_hash");
    if (runner_hash == record.end() || !runner_hash->is_string()) {
        return std::nullopt;
    }

    const auto files = record.find("files");
    if (files == record.end() || !files->is_object()) {
        return std::nullopt;
    }

    std::map<std::string, std::string> parsed_files;
    for (auto entry = files->begin(); entry != files->end(); ++entry) {
        if (!entry.value().is_string()) {
            return std::nullopt;
        }

        parsed_files.emplace(entry.key(), entry.value().get<std::string>());
    }

    return FingerprintRecord{
        .fingerprint = fingerprint->get<std::string>(),
        .runner_hash = runner_hash->get<std::string>(),
        .files = std::move(parsed_files),
    };
}
