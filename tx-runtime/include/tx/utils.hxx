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
[[nodiscard]] constexpr std::underlying_type_t<Enum> to_underlying(
    Enum enumeration
) noexcept {
    return static_cast<std::underlying_type_t<Enum>>(enumeration);
}

[[noreturn]] inline void unreachable(
    const source_location location = source_location::current()
) {
    if constexpr (IS_DEBUG_BUILD) {
        fmt::print(
            stderr,
            FMT_STRING("[{:s}:{:d}:{:d}] Error in {:s}():"
                       "This code should have been unreachable\n"),
            location.file_name(),
            location.line(),
            location.column(),
            location.function_name()
        );
        std::abort();
    }
    __builtin_unreachable();
}

template <typename F>
struct DeletableFacet : F {
    template <typename... Args>
    explicit DeletableFacet(Args&&... args) : F(std::forward<Args>(args)...) {}
    DeletableFacet(const DeletableFacet& other) = delete;
    DeletableFacet(DeletableFacet&& other) = delete;
    ~DeletableFacet() override = default;
    DeletableFacet* operator=(const DeletableFacet& other) = delete;
    DeletableFacet* operator=(DeletableFacet&& other) = delete;
};

inline char8_t*
utf8_encode(char32_t* src, char32_t* src_end, char8_t* dst, char8_t* dst_end) {
    using Facet = std::codecvt<char32_t, char8_t, std::mbstate_t>;
    std::mbstate_t mb_state{};
    const char32_t* src_next = nullptr;
    char8_t* dst_next = nullptr;
    auto facet = DeletableFacet<Facet>();
    auto out_r =
        facet.out(mb_state, src, src_end, src_next, dst, dst_end, dst_next);
    assert(out_r == std::codecvt_base::result::ok);
    return dst_next;
}

inline char8_t*
utf8_encode_n(char32_t* src, size_t n, char8_t* dst, char8_t* dst_end){
    return utf8_encode(src, src+n, dst, dst_end);
}

}  // namespace tx
