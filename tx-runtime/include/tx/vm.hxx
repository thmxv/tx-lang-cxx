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
    bool allow_pointer_to_source_content = true;
    bool allow_global_redefinition = false;
};

struct CallFrame {
    ObjFunction* function;
    const ByteCode* instruction_ptr;
    Value* slots;

    [[nodiscard]] constexpr ByteCode read_byte() noexcept;

    template<u32 N>
    [[nodiscard]] constexpr size_t read_multibyte_operand() noexcept {
        auto result = ::tx::read_multibyte_operand<N>(instruction_ptr);
        std::advance(instruction_ptr, N);
        return result;
    }

    template<size_t N>
    [[nodiscard]] constexpr Value read_constant() noexcept {
        const auto constant_idx = read_multibyte_operand<N>();
        return function->chunk.constants[size_cast(constant_idx)];
    }

    void print_instruction() const noexcept;
};

class Parser;
using CallFrames = FixedCapacityArray<CallFrame, size_t, FRAMES_MAX>;
using Stack = FixedCapacityArray<Value, size_t, STACK_MAX>;
using Allocator = std::pmr::polymorphic_allocator<std::byte>;

Value std_cpu_clock_native(VM& tvm, std::span<Value> args);
Value std_now_native(VM& tvm, std::span<Value> args);

class VM {
    VMOptions options{};
    Allocator allocator{};
    Parser* parser = nullptr;
    CallFrames frames{};
    Stack stack;
    ValueMap global_indices;
    ValueArray global_values;
    ValueSet strings;
    gsl::owner<Obj*> objects = nullptr;

  public:
    VM() = delete;

    // TODO: move declaration in _inl file
    VM(VMOptions opts, const Allocator& alloc) noexcept;
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

    // TX_VM_CONSTEXPR
    InterpretResult interpret(std::string_view source) noexcept;

    [[gnu::flatten]] TX_VM_CONSTEXPR InterpretResult run() noexcept;

  private:
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
    runtime_error(std::string_view fmt, Args&&... args) noexcept {
        fmt::print(stderr, fmt::runtime(fmt), std::forward<Args>(args)...);
        runtime_error_impl();
    }

    void print_stack() const noexcept;

    constexpr void debug_trace() const noexcept;

    constexpr size_t define_global(Value name, Value val) noexcept;

    // TODO: pass full signature, for the compiler to verify calls
    void define_native(std::string_view name, NativeFn fun) noexcept;

    [[nodiscard]] constexpr bool call(ObjFunction& fun, size_t arg_c) noexcept;

    [[nodiscard]] constexpr bool
    call_value(Value callee, size_t arg_c) noexcept;

    [[nodiscard]] constexpr bool negate_op() noexcept;

    template <template <typename> typename Op>
    [[nodiscard]] constexpr bool binary_op() noexcept;

    // Friends
    template <typename T, typename... Args>
    friend T*
    allocate_object_extra_size(VM& tvm, size_t extra, Args&&... args) noexcept;

    friend ObjString*
    make_string(VM& tvm, bool copy, std::string_view strv) noexcept;

    friend class Parser;
};

}  // namespace tx
