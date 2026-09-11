#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace string_utils {
std::string trim(std::string_view input);
std::vector<std::string> split(std::string_view input);
} // namespace string_utils
