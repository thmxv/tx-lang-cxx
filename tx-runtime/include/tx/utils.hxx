#pragma once

#include "tx/common.hxx"
#include "tx/exit_codes.hxx"

#include <fmt/format.h>

#include <cassert>
#include <type_traits>

// TODO remove and force clang 15
#ifdef __clang__
#include <experimental/source_location>
using source_location = std::experimental::source_location;
#else
#include <source_location>
using source_location = std::source_location;
#endif

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

[[noreturn]] inline void exit(ExitCode code) {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    std::exit(to_underlying(code));
}

[[noreturn]] inline void report_and_abort(
    std::string_view message,
    const source_location location = source_location::current()
) {
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
    const source_location location = source_location::current()
) {
    if constexpr (IS_DEBUG_BUILD) {
        report_and_abort("This code should have been unreachable", location);
    }
    __builtin_unreachable();
}

[[nodiscard]] inline constexpr bool has_integer_value(float_t val) {
    float_t int_part = 0.0;
    const auto fract_part = std::modf(val, &int_part);
    return fract_part == 0.0 && !std::isinf(int_part);
}

}  // namespace tx
