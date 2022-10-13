#pragma once

#include "tx/common.hxx"

#include <fmt/format.h>

#include <cassert>
#include <codecvt>

// TODO remove and force clang 15
#ifdef __clang__
#include <experimental/source_location>
using source_location = std::experimental::source_location;
#else
#include <source_location>
using source_location = std::source_location;
#endif

namespace tx {

template <typename Enum>
[[nodiscard]] inline constexpr std::underlying_type_t<Enum> to_underlying(
    Enum enumeration
) noexcept {
    return static_cast<std::underlying_type_t<Enum>>(enumeration);
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

template <typename F>
struct NoLocaleFacet : F {
    template <typename... Args>
    explicit NoLocaleFacet(Args&&... args) : F(std::forward<Args>(args)...) {}
    NoLocaleFacet(const NoLocaleFacet& other) = delete;
    NoLocaleFacet(NoLocaleFacet&& other) = delete;
    ~NoLocaleFacet() override = default;
    NoLocaleFacet* operator=(const NoLocaleFacet& other) = delete;
    NoLocaleFacet* operator=(NoLocaleFacet&& other) = delete;
};

// constexpr
[[nodiscard]] inline std::codecvt_base::result utf8_encode(
    const char32_t* src,
    const char32_t* src_end,
    const char32_t* src_next,
    char8_t* dst,
    char8_t* dst_end,
    char8_t* dst_next
) {
    using Facet = std::codecvt<char32_t, char8_t, std::mbstate_t>;
    std::mbstate_t mb_state{};
    auto facet = NoLocaleFacet<Facet>();
    return facet.out(mb_state, src, src_end, src_next, dst, dst_end, dst_next);
}

}  // namespace tx
