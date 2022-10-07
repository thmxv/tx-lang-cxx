#pragma once

#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/fixed_array.hxx"

namespace tx {

enum class InterpretResult {
    OK,
    COMPILE_ERROR,
    RUNTIME_ERROR,
};

using Stack = FixedCapacityArray<Value, size_t, STACK_MAX>;

class VM {
    bool trace_execution = true;
    const Chunk* chunk_ptr = nullptr;
    const ByteCode* instruction_ptr = nullptr;
    Stack stack;

  public:
    // VM();
    // ~VM();

    constexpr ByteCode read_byte() noexcept;
    constexpr Value read_constant(bool is_long) noexcept;

    constexpr InterpretResult interpret(const Chunk& chunk) noexcept;

    template<typename T, template<typename> typename Op>
    [[nodiscard]] constexpr bool binary_op() noexcept;

    constexpr InterpretResult run() noexcept;

    constexpr void push(Value value) noexcept { stack.push_back(value); }
    constexpr Value pop() noexcept {
        auto value = stack.back();
        stack.pop_back();
        return value;
    }
};

}  // namespace tx
