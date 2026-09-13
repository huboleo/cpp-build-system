#pragma once
#include <expected>
#include <string>

namespace bb {
enum class InitMode { NEW_PROJECT, EXISTING_PROJECT };

[[nodiscard]] std::expected<void, std::string> init(InitMode mode = InitMode::NEW_PROJECT);

}
