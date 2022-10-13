#pragma once

#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/compiler.hxx"
#include "tx/fixed_array.hxx"

#include <memory_resource>

namespace tx {

enum class InterpretResult {
    OK,
    COMPILE_ERROR,
    RUNTIME_ERROR,
};

struct VMOptions {
    bool trace_execution = false;
    bool print_tokens = false;
    bool print_bytecode = false;
    bool trace_gc = false;
    bool stress_gc = false;
};

class Parser;

using Stack = FixedCapacityArray<Value, size_t, STACK_MAX>;
using Allocator = std::pmr::polymorphic_allocator<std::byte>;

class VM {
    VMOptions options{};
    Allocator allocator{};
    Parser* parser = nullptr;
    const Chunk* chunk_ptr = nullptr;
    const ByteCode* instruction_ptr = nullptr;
    Stack stack{};

  public:
    // constexpr
    VM() noexcept = default;

    // constexpr
    explicit VM(VMOptions opts) noexcept : options(opts) {}

    constexpr explicit VM(const Allocator& alloc) noexcept : allocator(alloc) {}
    constexpr VM(VMOptions opts, const Allocator& alloc) noexcept
            : options(opts)
            , allocator(alloc) {}

    // constexpr
    [[nodiscard]] Allocator get_allocator() const { return allocator; }
    [[nodiscard]] constexpr const VMOptions& get_options() const noexcept {
        return options;
    }

    TX_VM_CONSTEXPR InterpretResult interpret(std::string_view source) noexcept;
    TX_VM_CONSTEXPR InterpretResult run(const Chunk& chunk) noexcept;

  private:
    constexpr ByteCode read_byte() noexcept;
    constexpr Value read_constant(bool is_long) noexcept;

    [[nodiscard]] constexpr bool negate_op() noexcept;

    template <template <typename> typename Op>
    [[nodiscard]] constexpr bool binary_op() noexcept;

    void print_stack() const noexcept;
    void print_instruction() const noexcept;
    constexpr void debug_trace() const noexcept;

    constexpr void push(Value value) noexcept { stack.push_back_unsafe(value); }
    constexpr Value pop() noexcept {
        auto value = stack.back();
        stack.pop_back();
        return value;
    }
};

}  // namespace tx
