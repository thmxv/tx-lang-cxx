#pragma once

#include "tx/common.hxx"
#include "tx/exit_codes.hxx"

#include <fmt/format.h>

#include <cassert>
#include <gsl/util>
#include <initializer_list>
#include <source_location>
#include <type_traits>

namespace tx {

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] inline constexpr size_t size_cast(T val) noexcept {
    return static_cast<size_t>(val);
}

template <typename Enum>
[[nodiscard]] inline constexpr std::underlying_type_t<Enum> to_underlying(
    Enum enumeration
) noexcept {
    return static_cast<std::underlying_type_t<Enum>>(enumeration);
}

[[noreturn]] inline void exit(ExitCode code) noexcept {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    std::exit(to_underlying(code));
}

[[noreturn]] inline void report_and_abort(
    std::string_view message,
    const std::source_location location = std::source_location::current()
) noexcept {
    fmt::print(
        stderr,
        FMT_STRING("[{:s}:{:d}:{:d}] Error in {:s}(): {}\n"),
        location.file_name(),
        location.line(),
        location.column(),
        location.function_name(),
        message
    );
    std::abort();
}

[[noreturn]] inline void unreachable(
    const std::source_location location = std::source_location::current()
) noexcept {
    if constexpr (IS_DEBUG_BUILD) {
        report_and_abort("This code should have been unreachable", location);
    }
    __builtin_unreachable();
}

[[nodiscard]] inline constexpr bool has_integer_value(float_t val) noexcept {
    float_t int_part = 0.0;
    const auto fract_part = std::modf(val, &int_part);
    return fract_part == 0.0 && !std::isinf(int_part);
}

// From:
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
[[nodiscard]] inline constexpr u32 power_of_2_ceil(u32 val) noexcept {
    val--;
    val |= val >> 1U;
    val |= val >> 2U;
    val |= val >> 4U;
    val |= val >> 8U;   // NOLINT(*-magic-numbers)
    val |= val >> 16U;  // NOLINT(*-magic-numbers)
    return ++val;
}

[[nodiscard]] inline constexpr i32 power_of_2_ceil(i32 val) noexcept {
    return gsl::narrow_cast<i32>(static_cast<std::make_unsigned_t<i32>>(val));
}

[[nodiscard]] inline constexpr size_t count_digit(size_t number) noexcept {
    size_t count = 0;
    while (number != 0) {
        number = number / 10;  // NOLINT(*-magic-numbers)
        ++count;
    }
    return count;
}

[[nodiscard]] inline constexpr std::string_view
get_text_of_line(std::string_view source, size_t line) {
    size_t current_line = 1;
    const char* begin = nullptr;
    const char* end = nullptr;
    for (const auto& chr : source) {
        if (current_line == line && begin == nullptr) { begin = &chr; }
        if (chr == '\n') {
            if (current_line == line) {
                end = &chr;
                break;
            }
            ++current_line;
        }
    }
    if (begin == nullptr) { begin = source.cbegin(); }
    if (end == nullptr) { end = source.cend(); }
    return {begin, end};
}

}  // namespace tx
