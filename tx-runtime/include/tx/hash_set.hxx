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
        :
        // TODO: using bool instead of value would lower memory during execution
        // investigate cost in binary bloat
        // public HashMap<T, bool, EMPTY_VALUE, false, true, Hash, Equal> {
        public HashMap<
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
        Equal
    >;

  public:
    constexpr bool set(VM& tvm, T val) noexcept {
        return Base::set(tvm, val, Value{val_none});
    }
};

}  // namespace tx
