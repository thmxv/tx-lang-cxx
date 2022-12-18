#pragma once

// This file will be generated automatically when you run the CMake
// configuration step. It creates a namespace called `tx::cmake`.
// You can modify the source template at `configured_files/config.hpp.in`.
#include "internal_use_only/config.hxx"

#include <cstddef>
#include <cstdint>

namespace tx {

#ifdef NDEBUG
inline constexpr bool IS_DEBUG_BUILD = false;
#define TX_ENABLE_COMPUTED_GOTO
#else
inline constexpr bool IS_DEBUG_BUILD = true;
#endif

#ifdef TX_ENABLE_COMPUTED_GOTO
#define TX_VM_CONSTEXPR
#else
#define TX_VM_CONSTEXPR constexpr
#endif

inline constexpr std::string_view VERSION = cmake::project_version;
inline constexpr int VERSION_MAJOR = cmake::project_version_major;
inline constexpr int VERSION_MINOR = cmake::project_version_minor;
inline constexpr int VERSION_PATCH = cmake::project_version_patch;
inline constexpr int VERSION_TWEAK = cmake::project_version_tweak;
inline constexpr std::string_view GIT_SHA = cmake::git_sha;

inline constexpr bool HAS_DEBUG_FEATURES = cmake::has_debug_features;

// FIXME: Better and more consistent naming
inline constexpr size_t FRAMES_START = 64;
inline constexpr size_t FRAMES_MAX = 1U << 10U;
inline constexpr size_t STACK_START = FRAMES_START * 256;
inline constexpr size_t LOCALS_MAX = 1U << 24U;
inline constexpr size_t MAX_UPVALUES = 1U << 24U;
inline constexpr size_t GC_START = size_t{1024} * 1024;

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
using f32 = float;
using f64 = double;

// NOTE: Configurable size type used in collections
// TODO: Test with signed/unsiged 32/64 integer and benchmark
using size_t = i32;
// using size_t = usize;

using int_t = i64;
using float_t = f64;
// TODO: allow building a minimal 32 bit version of Tx
// using int_t = i32;
// using float_t = f32;

}  // namespace tx
