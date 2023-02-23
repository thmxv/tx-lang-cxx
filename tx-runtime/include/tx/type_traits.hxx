#pragma once

#include <bits/stl_uninitialized.h>
#include <type_traits>

namespace tx {

template <typename T>
struct is_trivially_relocatable : std::__is_bitwise_relocatable<T> {};

template <typename T>
    requires T::IS_TRIVIALLY_RELOCATABLE
struct is_trivially_relocatable<T>
        : std::bool_constant<T::IS_TRIVIALLY_RELOCATABLE> {};

template <typename T>
inline constexpr bool is_trivially_relocatable_v =
    is_trivially_relocatable<T>::value;

// TODO: move to concept.hxx, or move all this file to utils.hxx?
// TODO: Better file names!
class VM;

template <typename T>
concept TxDestroyable = requires(T val, VM& tvm) {
                            { val.destroy(tvm) } -> std::same_as<void>;
                        };

template <typename T>
concept TxCopyable = requires(T val, VM& tvm) {
                         { val.copy(tvm) } -> std::same_as<T>;
                     };

}  // namespace tx
