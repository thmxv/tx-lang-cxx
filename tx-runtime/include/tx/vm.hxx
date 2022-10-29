#pragma once

#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/compiler.hxx"
#include "tx/fixed_array.hxx"

#include "tx/table.hxx"
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
    // REPL specific options
    bool allow_pointer_to_souce_content = true;
    bool allow_global_redefinition = false;
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
    Stack stack;
    ValueMap global_indices;
    ValueArray global_values;
    ValueSet strings;
    gsl::owner<Obj*> objects = nullptr;

  public:
    // constexpr
    VM() noexcept = default;

    // constexpr
    explicit VM(VMOptions opts) noexcept : options(opts) {}

    constexpr explicit VM(const Allocator& alloc) noexcept : allocator(alloc) {}
    constexpr VM(VMOptions opts, const Allocator& alloc) noexcept
            : options(opts)
            , allocator(alloc) {}

    VM(const VM& other) = delete;
    VM(VM&& other) = delete;

    constexpr ~VM() noexcept;

    VM& operator=(const VM& rhs) = delete;
    VM& operator=(VM&& rhs) = delete;

    // constexpr
    [[nodiscard]] Allocator get_allocator() const { return allocator; }

    [[nodiscard]] constexpr const VMOptions& get_options() const noexcept {
        return options;
    }

    [[nodiscard]] constexpr VMOptions& get_options() noexcept {
        return options;
    }

    TX_VM_CONSTEXPR InterpretResult interpret(std::string_view source) noexcept;
    TX_VM_CONSTEXPR InterpretResult run(const Chunk& chunk) noexcept;

  private:
    constexpr ByteCode read_byte() noexcept;
    constexpr size_t read_multibyte_index(bool is_long) noexcept;
    [[nodiscard]] constexpr u16 read_short() noexcept;
    constexpr Value read_constant(bool is_long) noexcept;

    constexpr void push(Value value) noexcept { stack.push_back_unsafe(value); }
    constexpr Value pop() noexcept {
        assert(!stack.empty());
        auto value = stack.back();
        stack.pop_back();
        return value;
    }
    constexpr Value peek(size_t distance) noexcept {
        assert(!stack.empty());
        return *std::prev(stack.end(), 1 + distance);
    }

    constexpr void reset_stack() noexcept;

    void runtime_error_impl() noexcept;

    template <typename... Args>
    void
    runtime_error(fmt::format_string<Args...> fmt, Args&&... args) noexcept {
        fmt::print(stderr, fmt, std::forward<Args>(args)...);
        runtime_error_impl();
    }

    [[nodiscard]] constexpr bool negate_op() noexcept;

    template <template <typename> typename Op>
    [[nodiscard]] constexpr bool binary_op() noexcept;

    void print_stack() const noexcept;
    void print_instruction() const noexcept;
    constexpr void debug_trace() const noexcept;

    template <typename T, typename... Args>
    friend T* allocate_object(VM& tvm, size_t extra, Args&&... args) noexcept;

    friend constexpr ObjString*
    make_string(VM& tvm, bool copy, std::string_view strv) noexcept;

    friend class Parser;
};

}  // namespace tx
