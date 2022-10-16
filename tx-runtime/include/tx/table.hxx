#pragma once

#include "tx/hash_set.hxx"
#include "tx/value.hxx"
// for Value::operator==()
#include "tx/value_inl.hxx"

namespace tx {

class Table
        : public HashSet<
              ObjString*,
              nullptr,
              Hash<ObjString*>,
              std::equal_to<ObjString*> > {
  public:
    [[nodiscard]] constexpr ObjString*
    find_string(std::string_view str_v, u32 hash) noexcept;
};

}  // namespace tx
