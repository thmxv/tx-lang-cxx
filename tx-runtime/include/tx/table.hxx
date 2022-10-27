#pragma once

#include "tx/hash_map.hxx"
#include "tx/hash_set.hxx"
#include "tx/value.hxx"

namespace tx {

using ValueMap =
    HashMap<Value, Value, Value{val_none}, Value{val_none}, Value{val_nil}>;

// using StringSet =
//     HashSet<ObjString*, nullptr, Hash<ObjString*>, std::equal_to<ObjString*>
//     >;

using ValueSet = HashSet<Value, Value{val_none}>;

}  // namespace tx
