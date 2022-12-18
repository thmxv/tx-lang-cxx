#pragma once

#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/compiler.hxx"
#include "tx/fixed_array.hxx"

#include "tx/table.hxx"
#include <fmt/core.h>
#include <memory_resource>
#include <string_view>

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
    // REPL specific options
    bool allow_pointer_to_source_content = true;
    bool allow_global_redefinition = false;
    bool allow_end_compile_with_undefined_global = false;
};

struct CallFrame {
    static constexpr bool IS_TRIVIALLY_RELOCATABLE = true;

    ObjClosure& closure;
    const ByteCode* instruction_ptr;
    Value* slots;

    constexpr CallFrame(
        ObjClosure& closure_,
        const ByteCode* iptr,
        Value* slots_
    ) noexcept
            : closure(closure_)
            , instruction_ptr(iptr)
            , slots(slots_) {}

    [[nodiscard]] constexpr ByteCode read_byte() noexcept;

    template <u32 N>
    [[nodiscard]] constexpr size_t read_multibyte_operand() noexcept {
        auto result = ::tx::read_multibyte_operand<N>(instruction_ptr);
        std::advance(instruction_ptr, N);
        return result;
    }

    template <size_t N>
    [[nodiscard]] constexpr Value read_constant() noexcept {
        const auto constant_idx = read_multibyte_operand<N>();
        return closure.function.chunk.constants[size_cast(constant_idx)];
    }

    [[nodiscard]] constexpr std::tuple<bool, size_t> read_closure_operand(
    ) noexcept;

    void print_instruction() const noexcept;
};

class Parser;

using Allocator = std::pmr::polymorphic_allocator<std::byte>;

class VM {
    using CallFrames = DynArray<CallFrame>;
    using Stack = DynArray<Value>;
    using GlobalArray = DynArray<Global>;
    using GrayStack = DynArray<Obj*, size_t, false>;

    VMOptions options{};
    Allocator allocator{};
    Parser* parser{nullptr};
    CallFrames frames{};
    Stack stack;
    ValueArray global_values;
    ValueSet strings;
    ObjUpvalue* open_upvalues{nullptr};
    size_t bytes_allocated{0};
    size_t next_gc{GC_START};
    gsl::owner<Obj*> objects = nullptr;
    GrayStack gray_stack;

    // Only strictly needed by the parser,
    // but need to persist for REPL and error messages
    ValueMap global_indices;
    GlobalArray global_signatures;

  public:
    VM() = delete;
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

    inline ObjFunction* compile(std::string_view source) noexcept;

    // TX_VM_CONSTEXPR
    [[gnu::flatten]] InterpretResult run() noexcept;

  private:
    constexpr void push(Value value) noexcept {
        assert(stack.capacity() > stack.size());
        stack.push_back_unsafe(value);
    }

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
        fmt::print(stderr, fmt, args...);
        runtime_error_impl();
    }

    void print_stack() const noexcept;
    void assert_stack_effect(const ByteCode* iptr) const noexcept;
    void debug_trace(const ByteCode* iptr) const noexcept;

    constexpr size_t
    define_global(Value name, Global signature, Value val) noexcept;

    [[nodiscard]] constexpr std::string_view get_global_name(size_t index
    ) const noexcept;

    // TODO: pass full signature, for the compiler to verify calls
    void define_native(std::string_view name, NativeFn fun) noexcept;

    void constexpr ensure_stack_space(i32 needed) noexcept;

    [[nodiscard]] constexpr bool
    call(ObjClosure& closure, size_t arg_c) noexcept;

    [[nodiscard]] constexpr bool
    call_value(Value callee, size_t arg_c) noexcept;

    [[nodiscard]] ObjUpvalue& capture_upvalue(Value* local) noexcept;

    constexpr void close_upvalues(const Value* last) noexcept;

    [[nodiscard]] constexpr bool negate_op() noexcept;

    template <template <typename> typename Op>
    [[nodiscard]] constexpr bool binary_op() noexcept;

    template <u8 N>
    inline void do_constant(CallFrame*& frame) noexcept;

    template <u8 N>
    inline void do_get_local(CallFrame*& frame) noexcept;

    template <u8 N>
    inline void do_set_local(CallFrame*& frame) noexcept;

    template <u8 N>
    inline bool do_get_global(CallFrame*& frame) noexcept;

    template <u8 N>
    inline void do_define_global(CallFrame*& frame) noexcept;

    template <u8 N>
    inline bool do_set_global(CallFrame*& frame) noexcept;

    template <u8 N>
    inline void do_get_upvalue(CallFrame*& frame) noexcept;

    template <u8 N>
    inline void do_set_upvalue(CallFrame*& frame) noexcept;

    template <u8 N>
    inline void do_closure(CallFrame*& frame) noexcept;

    template <u8 N>
    inline void do_end_scope(CallFrame*& frame) noexcept;

    // Friends
    friend constexpr void mark_roots(VM& tvm) noexcept;
    friend constexpr void mark_compiler_roots(VM& tvm) noexcept;
    friend constexpr void mark_object(VM& tvm, Obj* obj) noexcept;
    friend constexpr void trace_references(VM& tvm) noexcept;
    friend constexpr void sweep(VM& tvm) noexcept;
    friend constexpr void collect_garbage(VM& tvm) noexcept;

    template <bool TRIGGER_GC>
    // constexpr
    friend inline void* reallocate_impl(
        VM& tvm,
        void* pointer,
        size_t old_size,
        size_t new_size,
        size_t alignment
    ) noexcept;

    template <typename T, typename... Args>
    friend T*
    allocate_object_extra_size(VM& tvm, size_t extra, Args&&... args) noexcept;

    friend ObjString*
    make_string(VM& tvm, bool copy, std::string_view strv) noexcept;

    friend class Parser;
};

}  // namespace tx
