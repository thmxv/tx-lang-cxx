#pragma once

#include "tx/hash_map.hxx"
#include "tx/hash_set.hxx"
#include "tx/value.hxx"

namespace tx {

using ValueMap = HashMap<
    Value,
    Value,
    Value{val_none},
    Value{val_none},
    Value{val_nil},
    Hash<ObjString*>,
    std::equal_to<ObjString*> >;

using StringSet =
    HashSet<ObjString*, nullptr, Hash<ObjString*>, std::equal_to<ObjString*> >;
}  // namespace tx
