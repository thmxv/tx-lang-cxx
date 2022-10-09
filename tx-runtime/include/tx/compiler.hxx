#pragma once

#include "tx/vm.hxx"

#include <string_view>

namespace tx {

inline void compile(VM& tvm, std::string_view source);

}
