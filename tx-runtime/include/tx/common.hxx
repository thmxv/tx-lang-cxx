#pragma once

// This file will be generated automatically when you run the CMake
// configuration step. It creates a namespace called `tx::cmake`.
// You can modify the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hxx>

#include <cstddef>
#include <cstdint>

namespace tx {

    inline constexpr std::string_view version = cmake::project_version;
    inline constexpr int version_major = cmake::project_version_major;
    inline constexpr int version_minor = cmake::project_version_minor;
    inline constexpr int version_patch = cmake::project_version_patch;
    inline constexpr int version_tweak = cmake::project_version_tweak;
    inline constexpr std::string_view git_sha = cmake::git_sha;

    // Usefull short type aliases
    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    using isize = std::ptrdiff_t;
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using usize = std::size_t;

    // NOTE: Configurable size type used in all the code
    using size_t = i32;

}
