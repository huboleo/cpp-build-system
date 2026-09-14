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

    append_u64(encoded, inputs.command.size());
    for (const auto& argument : inputs.command) {
        append_framed(encoded, argument);
    }

    append_u64(encoded, inputs.files.size());
    for (const auto& [path, hash] : inputs.files) {
        append_framed(encoded, path);
        append_framed(encoded, hash);
    }

    return hash_bytes(encoded);
}

std::expected<void, std::string>
bb::write_fingerprint_record(const std::filesystem::path& path, const FingerprintInputs& inputs) {
    const nlohmann::json record{
        {"fingerprint_version", std::string{FINGERPRINT_VERSION}},
        {"fingerprint", compute_fingerprint(inputs)},
        {"command", inputs.command},
        {"files", inputs.files},
    };

    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) {
        return std::unexpected("Cannot open fingerprint record: " + path.string());
    }

    output << record.dump(2) << "\n";
    output.close();

    if (!output) {
        return std::unexpected("Cannot write fingerprint record: " + path.string());
    }

    return {};
}

std::optional<std::string> bb::read_fingerprint_record(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::nullopt;
    }

    const auto record = nlohmann::json::parse(input, nullptr, false);
    if (!record.is_object()) {
        return std::nullopt;
    }

    const auto fingerprint = record.find("fingerprint");
    if (fingerprint == record.end() || !fingerprint->is_string()) {
        return std::nullopt;
    }

    return fingerprint->get<std::string>();
}
