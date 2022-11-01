#pragma once

#include "tx/hash_map.hxx"
#include "tx/value.hxx"

namespace tx {

template <
    typename T,
    T EMPTY_VALUE,
    typename Hash = Hash<T>,
    typename Equal = std::equal_to<T> >
class HashSet
        : public HashMap<
              T,
              Value,
              EMPTY_VALUE,
              Value{val_none},
              Value{val_nil},
              Hash,
              Equal> {
    using Base = HashMap<
        T,
        Value,
        EMPTY_VALUE,
        Value{val_none},
        Value{val_nil},
        Hash,
        Equal>;

  public:
    constexpr bool set(VM& tvm, T val) noexcept {
        return Base::set(tvm, val, Value{val_none});
    }
};

}  // namespace tx
