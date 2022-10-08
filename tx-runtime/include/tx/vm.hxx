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

struct VMOptions {
    bool trace_execution = false;
    bool print_bytecode = false;
    bool trace_gc = false;
    bool stress_gc = false;
};

using Stack = FixedCapacityArray<Value, size_t, STACK_MAX>;

class VM {
    VMOptions options{};
    const Chunk* chunk_ptr = nullptr;
    const ByteCode* instruction_ptr = nullptr;
    Stack stack;

  public:
    VM();
    VM(VMOptions opts) noexcept : options(opts) {}

    constexpr ByteCode read_byte() noexcept;
    constexpr Value read_constant(bool is_long) noexcept;

    TX_VM_CONSTEXPR InterpretResult interpret(const Chunk& chunk) noexcept;

    template <typename T, template <typename> typename Op>
    [[nodiscard]] constexpr bool binary_op() noexcept;

    void print_stack() const noexcept;
    void print_instruction() const noexcept;
    constexpr void debug_trace() const noexcept;

    TX_VM_CONSTEXPR InterpretResult run(const Chunk& chunk) noexcept;

    constexpr void push(Value value) noexcept { stack.push_back_unsafe(value); }
    constexpr Value pop() noexcept {
        auto value = stack.back();
        stack.pop_back();
        return value;
    }
};

}  // namespace tx
