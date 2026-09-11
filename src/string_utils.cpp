#include "string_utils.hpp"
#include <cctype>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

std::string string_utils::trim(std::string_view input) {
    auto is_whitespace = [](char character) {
        return std::isspace(static_cast<unsigned char>(character));
    };

    auto trimmed = input | std::views::drop_while(is_whitespace) | std::views::reverse |
                   std::views::drop_while(is_whitespace) | std::views::reverse;
    return {trimmed.begin(), trimmed.end()};
}

std::vector<std::string> string_utils::split(std::string_view input) {
    auto normalized = input | std::views::transform([](char character) {
                          const auto value = static_cast<unsigned char>(character);
                          return std::isspace(value) ? ' ' : character;
                      });

    auto words = normalized | std::views::split(' ') |
                 std::views::filter([](const auto& part) { return !part.empty(); });

    std::vector<std::string> result;
    for (const auto& word : words) {
        result.emplace_back(word.begin(), word.end());
    }
    return result;
}
