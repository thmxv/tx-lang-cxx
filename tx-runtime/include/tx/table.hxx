#pragma once

#include "tx/hash_map.hxx"
#include "tx/value.hxx"
// for Value::operator==()
#include "tx/value_inl.hxx"

namespace tx {

class Table
        : public HashMap<
              ObjString*,
              Value,
              nullptr,
              Value{},
              Value(true),
              Hash<ObjString*>,
              std::equal_to<ObjString*> > {
  public:
    [[nodiscard]] constexpr ObjString*
    find_string(std::string_view str_v, u32 hash) noexcept;
};

}  // namespace tx
