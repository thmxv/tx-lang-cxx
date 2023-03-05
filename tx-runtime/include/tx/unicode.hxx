#pragma once

#include "tx/fixed_array.hxx"

#include <codecvt>
#include <utility>

namespace tx {

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
    const char32_t*& src_next,
    char* dst,
    char* dst_end,
    char*& dst_next
) {
    char8_t* dst_next8 = nullptr;
    using Facet = std::codecvt<char32_t, char8_t, std::mbstate_t>;
    std::mbstate_t mb_state{};
    auto facet = NoLocaleFacet<Facet>();
    auto result = facet.out(
        mb_state,
        src,
        src_end,
        src_next,
        static_cast<char8_t*>(static_cast<void*>(dst)),
        static_cast<char8_t*>(static_cast<void*>(dst_end)),
        dst_next8
    );
    dst_next = static_cast<char*>(static_cast<void*>(dst_next8));
    return result;
}

[[nodiscard]] inline FixedCapacityArray<char, size_t, 4> utf8_encode_single(
    char32_t val
) {
    FixedCapacityArray<char, size_t, 4> result(4, u'\0');
    const char32_t* src_next = nullptr;
    char* tmp_next = nullptr;
    auto enc_res = utf8_encode(
        &val,
        std::next(&val),
        src_next,
        result.begin(),
        result.end(),
        tmp_next
    );
    if (enc_res != std::codecvt_base::result::ok) { result.clear(); }
    result.resize(size_cast(std::distance(result.begin(), tmp_next)));
    return result;
}

}  // namespace tx
