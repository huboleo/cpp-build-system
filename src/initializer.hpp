#pragma once
#include <expected>
#include <string>

namespace bb {
[[nodiscard]] std::expected<void, std::string> init();

}
