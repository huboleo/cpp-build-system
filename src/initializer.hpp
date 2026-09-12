#pragma once
#include <expected>
#include <string>

namespace bb {
enum class InitMode { new_project, existing_project };

[[nodiscard]] std::expected<void, std::string> init(InitMode mode = InitMode::new_project);

}
