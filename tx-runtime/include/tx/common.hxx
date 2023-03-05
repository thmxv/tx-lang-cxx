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

// NOTE: Configurable, make cmake options
// FIXME: Better naming
inline constexpr size_t START_FRAMES = 64;
inline constexpr size_t MAX_FRAMES = 1U << 10U;
inline constexpr size_t START_STACK = START_FRAMES * 256;
inline constexpr size_t START_GC = size_t{1024} * 1024;

// NOTE: Not configurable, do not edit values
inline constexpr size_t MAX_LOCALS = 1U << 24U;
inline constexpr size_t MAX_UPVALUES = 1U << 24U;
inline constexpr size_t MAX_FN_PARAMETERS = 255;

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

// NOTE: Use 64 bits int/float by default, like Lua. Even on 32bit platforms.
// Optionally allow building a minimal 32 bit version of Tx (TODO)
using int_t = i64;
using float_t = f64;
// using int_t = i32;
// using float_t = f32;

// NOTE: Configurable size type used in collections
// TODO: Test with signed/unsiged 32/64 integer and benchmark perf and mem
using size_t = isize;
// using size_t = i32;
// using size_t = usize;
// using size_t = u32;

}  // namespace tx
