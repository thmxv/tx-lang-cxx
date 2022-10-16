#pragma once

#include "tx/hash_map.hxx"
#include "tx/hash_set.hxx"
#include "tx/value.hxx"

namespace tx {

using Map = HashMap<
    Value,
    Value,
    Value{},
    Value{},
    Value{true},
    Hash<ObjString*>,
    std::equal_to<ObjString*> >;

using StringSet =
    HashSet<ObjString*, nullptr, Hash<ObjString*>, std::equal_to<ObjString*> >;
}  // namespace tx
