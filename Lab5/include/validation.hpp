#pragma once
#include <charconv>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>

namespace lab {
inline void require(bool condition, const std::string& message) {
    if (!condition) throw std::invalid_argument(message);
}
inline std::string argument(int argc, char** argv, int& index, const std::string& key) {
    require(index + 1 < argc && std::string(argv[index + 1]).rfind("--", 0) != 0,
            "Missing value for " + key + ". Use --help for usage.");
    return argv[++index];
}
template<class T> T number(const std::string& text, const std::string& key) {
    T result{};
    require(!text.empty() && text.front() != '-' && text.front() != '+',
            key + " must be a nonnegative integer.");
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(),
            "Invalid or out-of-range integer for " + key + ": " + text);
    return result;
}
template<class T> void positive(T value, const std::string& key) {
    require(value > 0, key + " must be greater than zero.");
}
inline void choice(const std::string& value, const std::string& key,
                   std::initializer_list<const char*> allowed) {
    std::string options;
    for (auto option : allowed) {
        if (value == option) return;
        if (!options.empty()) options += ", ";
        options += option;
    }
    throw std::invalid_argument("Invalid " + key + ": " + value + ". Choose " + options + ".");
}
inline void matrix_size(std::size_t rows, std::size_t cols, std::size_t bytes,
                        const std::string& name) {
    require(cols == 0 || rows <= std::numeric_limits<std::size_t>::max() / bytes / cols,
            name + " dimensions exceed the addressable size.");
}
}
