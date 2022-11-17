#pragma once

#include "tx/common.hxx"

#include <bit>
#include <cstddef>
#include <cstring>
#include <span>

namespace tx {

inline constexpr uint32_t C1_ = 0xcc9e2d51;
inline constexpr uint32_t C2_ = 0x1b873593;
inline constexpr uint32_t MX_ = 0xe6546b64;
inline constexpr uint32_t F1_ = 0x85ebca6b;
inline constexpr uint32_t F2_ = 0xc2b2ae35;

[[gnu::flatten]] inline constexpr u32 murmur_32_scramble(u32 val) {
    val *= C1_;
    val = std::rotl(val, 15U);
    val *= C2_;
    return val;
}

inline constexpr u32 murmur_32_fmix(u32 val) {
    val ^= val >> 16U;
    val *= F1_;
    val ^= val >> 13U;
    val *= F2_;
    val ^= val >> 16U;
    return val;
}

[[gnu::flatten]] inline constexpr u32
murmur3_32(const std::span<const std::byte> key, u32 seed = 0) {
    constexpr u32 BLOCK_SIZE = 4;
    const u32 len = static_cast<u32>(key.size());
    const u32 num_blocks = len / BLOCK_SIZE;
    u32 hash = seed;
    // Body
    auto iter = key.begin();
    auto end = std::next(
        iter,
        static_cast<std::ptrdiff_t>(num_blocks) * BLOCK_SIZE
    );
    while (iter != end) {
        u32 blk = 0;
        std::memcpy(&blk, &*iter, BLOCK_SIZE);
        hash ^= murmur_32_scramble(blk);
        hash = std::rotl(hash, 13U);
        hash = hash * 5U + MX_;
        iter += BLOCK_SIZE;
    }
    // Tail
    u32 k_blk = 0;
    switch (len & (BLOCK_SIZE - 1)) {
        case 3:
            k_blk ^= static_cast<u32>(std::bit_cast<u8>(*std::next(iter, 2)))
                     << 16U;
            [[fallthrough]];
        case 2:
            k_blk ^= static_cast<u32>(std::bit_cast<u8>(*std::next(iter, 1)))
                     << 8U;
            [[fallthrough]];
        case 1:
            k_blk ^= std::bit_cast<u8>(*iter);
            hash ^= murmur_32_scramble(k_blk);
    }
    // Finalize.
    hash ^= len;
    hash = murmur_32_fmix(hash);
    return hash;
}

}  // namespace tx
