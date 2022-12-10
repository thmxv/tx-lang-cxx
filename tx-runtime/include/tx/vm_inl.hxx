#pragma once

#include "tx/vm.hxx"
//
#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/compiler.hxx"
#include "tx/debug.hxx"
#include "tx/formatting.hxx"
#include "tx/object.hxx"
#include "tx/utils.hxx"
#include "tx/value.hxx"

#include <fmt/format.h>

#include <chrono>
#include <cfenv>
#include <cstddef>
#include <ctime>
#include <iterator>
#include <functional>
#include <ranges>
#include <thread>
#include <type_traits>

namespace tx {

inline NativeResult core_version_string_native(VM& tvm, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    inout.return_value() = Value{make_string(tvm, false, VERSION)};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
core_version_major_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    inout.return_value() = Value{static_cast<int_t>(VERSION_MAJOR)};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
core_version_minor_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    inout.return_value() = Value{static_cast<int_t>(VERSION_MINOR)};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
core_version_patch_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    inout.return_value() = Value{static_cast<int_t>(VERSION_PATCH)};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
core_version_tweak_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    inout.return_value() = Value{static_cast<int_t>(VERSION_TWEAK)};
    return NativeResult::SUCCESS;
}

inline NativeResult core_assert_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.size() == 2);
    assert(args[1].is_object() && args[1].as_object().is_string());
    if (args[0].is_falsey()) {
        inout.return_value() = Value{args[1]};
        return NativeResult::RUNTIME_ERROR;
    }
    inout.return_value() = Value{val_nil};
    return NativeResult::SUCCESS;
}

inline NativeResult std_cpu_clock_read_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    inout.return_value() = Value{std::clock()};
    return NativeResult::SUCCESS;
}

inline NativeResult
std_cpu_clock_elapsed_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.size() == 1);
    assert(args[0].is_int());
    const auto start = args[0].as_int();
    const auto end = std::clock();
    inout.return_value() = Value{
        static_cast<float_t>(end - start) / CLOCKS_PER_SEC};
    return NativeResult::SUCCESS;
}

using DurationInt = std::chrono::duration<int_t, std::micro>;
using DurationFloat = std::chrono::duration<float_t>;
using TimePoint =
    std::chrono::time_point<std::chrono::high_resolution_clock, DurationInt>;
inline NativeResult std_wall_clock_read_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    const auto val = time_point_cast<DurationInt>(
        std::chrono::high_resolution_clock::now()
    );
    inout.return_value() = Value{std::bit_cast<int_t>(val)};
    return NativeResult::SUCCESS;
}

inline NativeResult
std_wall_clock_elapsed_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.size() == 1);
    assert(args[0].is_int());
    const auto start = std::bit_cast<TimePoint>(args[0].as_int());
    const auto end = time_point_cast<DurationInt>(
        std::chrono::high_resolution_clock::now()
    );
    const DurationFloat elapsed = (end - start);
    inout.return_value() = Value{elapsed.count()};
    return NativeResult::SUCCESS;
}

inline NativeResult std_sleep_for_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.size() == 1);
    assert(args[0].is_number());
    // FIXME: use a user supplied function to support freestanding/bare-metal
    std::this_thread::sleep_for(
        std::chrono::duration<float_t>(args[0].as_float_force())
    );
    inout.return_value() = Value{val_nil};
    return NativeResult::SUCCESS;
}

inline NativeResult std_println_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.size() == 1);
    fmt::print("{}\n", args[0]);
    inout.return_value() = Value{val_nil};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
float_has_integer_value_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.size() == 1);
    assert(args[0].is_float());
    inout.return_value() = Value{has_integer_value(args[0].as_float())};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
float_sqrt_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.size() == 1);
    assert(args[0].is_float());
    const auto val = args[0].as_float();
    const auto res = std::sqrt(val);
    // FIXME: we should save/restore except flags instead of just clearing
    // after. Or maybe just do not care and let them propagate
    if (std::isnan(res)) { std::feclearexcept(FE_INVALID); }
    inout.return_value() = Value{res};
    return NativeResult::SUCCESS;
}

inline VM::VM(VMOptions opts, const Allocator& alloc) noexcept
        : options(opts)
        , allocator(alloc) {
    if constexpr (!IS_DEBUG_BUILD) {
        frames.reserve(*this, FRAMES_START);
        stack.reserve(*this, STACK_START);
    }

    define_native("core_version_string", core_version_string_native);
    define_native("core_version_major", core_version_major_native);
    define_native("core_version_minor", core_version_minor_native);
    define_native("core_version_patch", core_version_patch_native);
    define_native("core_version_tweak", core_version_tweak_native);
    define_native("core_assert", core_assert_native);
    define_native("std_cpu_clock_read", std_cpu_clock_read_native);
    define_native("std_cpu_clock_elapsed", std_cpu_clock_elapsed_native);
    define_native("std_wall_clock_read", std_wall_clock_read_native);
    define_native("std_wall_clock_elapsed", std_wall_clock_elapsed_native);
    define_native("std_sleep_for", std_sleep_for_native);
    define_native("std_println", std_println_native);
    define_native("Float_has_integer_value", float_has_integer_value_native);
    define_native("Float_sqrt", float_sqrt_native);
}

inline constexpr VM::~VM() noexcept {
    stack.destroy(*this);
    frames.destroy(*this);
    global_indices.destroy(*this);
    global_values.destroy(*this);
    globals.destroy(*this);
    strings.destroy(*this);
    free_objects(*this, objects);
}

[[nodiscard]] inline constexpr ByteCode CallFrame::read_byte() noexcept {
    auto result = *instruction_ptr;
    instruction_ptr = std::next(instruction_ptr);
    return result;
}

inline constexpr void VM::reset_stack() noexcept {
    stack.clear();
    frames.clear();
    if constexpr (IS_DEBUG_BUILD) {
        stack.destroy(*this);
        frames.destroy(*this);
    }
}

inline void VM::runtime_error_impl() noexcept {
    fmt::print(stderr, "\n");
    for (size_t i = frames.size() - 1; i >= 0; --i) {
        const auto& frame = frames[i];
        const auto& function = *frame.function;
        auto instruction = frame.instruction_ptr - function.chunk.code.begin()
                           - 1;
        fmt::print(
            stderr,
            "[line {:d}] in {:s}\n",
            function.chunk.get_line(static_cast<i32>(instruction)),
            function.get_display_name()
        );
    }
    reset_stack();
}

// TX_VM_CONSTEXPR
inline InterpretResult VM::interpret(std::string_view source) noexcept {
    Parser current_parser(*this, source);
    parser = &current_parser;
    ObjFunction* function = parser->compile();
    parser = nullptr;
    if (function == nullptr) { return InterpretResult::COMPILE_ERROR; }
    ensure_stack_space(1);
    push(Value{function});
    (void)call(*function, 0);
    auto result = run();
    assert(stack.empty());
    return result;
}

inline constexpr size_t
VM::define_global(Value name, GlobalInfo signature, Value val) noexcept {
    // NOTE: maybe do the GC push/pop dance here
    auto new_index = global_values.size();
    global_values.push_back(*this, val);
    globals.push_back(*this, signature);
    global_indices.set(*this, name, Value{static_cast<int_t>(new_index)});
    return new_index;
}

[[nodiscard]] constexpr std::string_view VM::get_global_name(size_t index
) const noexcept {
    assert(index < global_values.size());
    // auto iter = std::find_if(
    //     global_indices.cbegin(),
    //     global_indices.cend(),
    //     // global_indices,
    //     [=](const auto& entry) { return entry.second.as_int() == index; }
    // );
    // assert(iter != global_indices.cend());
    // return iter->first.as_object().as<ObjString>();
    for (const auto& entry : global_indices) {
        // cppcheck-suppress useStlAlgorithm
        if (entry.second.as_int() == index) {
            return entry.first.as_object().as<ObjString>();
        }
    }
    unreachable();
}

inline void VM::define_native(std::string_view name, NativeFn fun) noexcept {
    assert(stack.empty());
    ensure_stack_space(2);
    push(Value{make_string(*this, false, name)});
    push(Value{allocate_object<ObjNative>(*this, fun)});
    define_global(
        stack[0],
        GlobalInfo{
            .signature = GlobalSignature{.is_const = true},
            .is_defined = true},
        stack[1]
    );
    pop();
    pop();
}

inline constexpr void VM::ensure_stack_space(i32 needed) noexcept {
    if (stack.capacity() >= needed) { return; }
    auto* old = stack.begin();
    if constexpr (IS_DEBUG_BUILD) {
        stack.reserve(*this, needed);
    } else {
        stack.reserve(*this, power_of_2_ceil(needed));
    }
    if (old != stack.cbegin()) {
        for (auto& frame : frames) {
            frame.slots = std::next(
                stack.begin(),
                std::distance(old, frame.slots)
            );
        }
    }
}

[[nodiscard]] inline constexpr bool
VM::call(ObjFunction& fun, size_t arg_c) noexcept {
    // TODO: make this a compile time error
    if (arg_c != fun.arity) {
        runtime_error(
            "Expected {:d} arguments but got {:d}.",
            fun.arity,
            arg_c
        );
        return false;
    }
    if (frames.size() == FRAMES_MAX) {
        runtime_error("Stack overflow.");
        return false;
    }
    const auto stack_needed = stack.size() + fun.max_slots;
    ensure_stack_space(stack_needed);
    frames.push_back(
        *this,
        CallFrame{
            .function = &fun,
            .instruction_ptr = fun.chunk.code.begin(),
            .slots = std::prev(stack.end(), arg_c + 1)}
    );
    return true;
}

[[nodiscard]] inline constexpr bool
VM::call_value(Value callee, size_t arg_c) noexcept {
    if (callee.is_object()) {
        auto& obj = callee.as_object();
        switch (obj.type) {
            using enum ObjType;
            case FUNCTION: return call(obj.as<ObjFunction>(), arg_c);
            case NATIVE: {
                auto& native = obj.as<ObjNative>();
                auto success = std::invoke(
                    native.function,
                    *this,
                    NativeInOut(std::prev(stack.end(), arg_c + 1), arg_c + 1)
                );
                if (success == NativeResult::SUCCESS) {
                    // Return value already put in right slot by native func.
                    // Do not erase it :)
                    stack.erase(std::prev(stack.cend(), arg_c), stack.cend());
                    return true;
                }
                const auto& return_value = *std::prev(stack.cend(), arg_c + 1);
                runtime_error(return_value.as_object().as<ObjString>());
                return false;
            }
            default: break;
        }
    }
    // TODO: make this a compile time error
    runtime_error("Can only call functions.");
    return false;
}

[[nodiscard]] inline constexpr bool VM::negate_op() noexcept {
    Value result{val_none};
    if (peek(0).is_int()) {
        result = Value(-pop().as_int());
    } else if (peek(0).is_float()) {
        result = Value(-pop().as_float());
    } else {
        // TODO: make this a compile time error
        runtime_error("Operand must be a number.");
        return true;
    }
    push(result);
    return false;
}

template <template <typename> typename Op>
[[nodiscard]] constexpr bool VM::binary_op() noexcept {
    if (!peek(0).is_number() || !peek(1).is_number()) {
        // TODO: make this a compile time error
        runtime_error("Operands must be numbers.");
        return true;
    }
    const auto rhs = pop();
    const auto lhs = pop();
    Value result{val_none};
    if (lhs.is_int() && rhs.is_int()) {
        Op<int_t> bop;
        result = Value(bop(lhs.as_int(), rhs.as_int()));
    } else {
        float_t left = lhs.as_float_force();
        float_t rght = rhs.as_float_force();
        Op<float_t> bop;
        result = Value(bop(left, rght));
    }
    push(result);
    return false;
}

inline void VM::print_stack() const noexcept {
    fmt::print(FMT_STRING("          "));
    for (const auto& slot : stack) { fmt::print(FMT_STRING("[ {} ]"), slot); }
    fmt::print(FMT_STRING("\n"));
}

inline void CallFrame::print_instruction() const noexcept {
    disassemble_instruction(function->chunk, instruction_ptr);
}

// FIXME: Make sure this does not bloat the binary in realease mode
inline void VM::assert_stack_effect(const ByteCode* iptr) const noexcept {
    static auto previous_size = 0;
    static auto previous_opc = OpCode::END;
    static size_t previous_operand = 0;
    const auto current_size = stack.size();
    const auto delta = current_size - previous_size;

    switch (previous_opc) {
        using enum OpCode;
        // FIXME: verify CALL and RETURN if possible
        case END:
        case CALL:
        case RETURN: break;
        default: {
            assert(
                delta == get_opcode_stack_effect(previous_opc, previous_operand)
            );
        }
    }
    previous_size = current_size;
    previous_opc = (*iptr).as_opcode();
    const auto operand_size = get_byte_count_following_opcode(previous_opc);
    if (operand_size == 1) {
        previous_operand = ::tx::read_multibyte_operand<1>(std::next(iptr));
    } else if (operand_size == 2) {
        previous_operand = ::tx::read_multibyte_operand<2>(std::next(iptr));
    } else if (operand_size == 3) {
        previous_operand = ::tx::read_multibyte_operand<3>(std::next(iptr));
    }
}

inline void VM::debug_trace(const ByteCode* iptr) const noexcept {
    if constexpr (HAS_DEBUG_FEATURES) {
        if (options.trace_execution) {
            print_stack();
            frames.back().print_instruction();
        }
    }
    if constexpr (IS_DEBUG_BUILD) { assert_stack_effect(iptr); }
}

// TX_VM_CONSTEXPR
[[gnu::flatten]] inline InterpretResult VM::run() noexcept {
// clang-format off
    #ifdef TX_ENABLE_COMPUTED_GOTO
        __extension__
        static void* dispatch_table[] = {
            #define TX_OPCODE(name, length, _) &&L_opcode_##name,
            #include "tx/opcodes.inc"
            #undef TX_OPCODE
        };
        #define TX_VM_DISPATCH TX_VM_BREAK();
        #define TX_VM_CASE(name) L_opcode_##name
        #define TX_VM_BREAK() \
            do { \
                debug_trace(frame->instruction_ptr); \
                instruction = frame->read_byte().as_opcode(); \
                (__extension__( \
                    {goto *dispatch_table[to_underlying(instruction)];} \
                )); \
            } while (false)
    #else
        #define TX_VM_DISPATCH  \
            debug_trace(frame->instruction_ptr); \
            instruction = frame->read_byte().as_opcode(); \
            switch (instruction)
        // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
        #define TX_VM_CASE(name) case name
        // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
        #define TX_VM_BREAK() break
    #endif
    // clang-format on

    // Try to force the compiler to store it in a register
    CallFrame* frame = &frames.back();
    OpCode instruction = OpCode::END;
    for (;;) {
        TX_VM_DISPATCH {
            using enum OpCode;
            TX_VM_CASE(CONSTANT) : {
                constexpr auto count = get_byte_count_following_opcode(CONSTANT
                );
                static_assert(count == 1);
                push(frame->read_constant<count>());
                TX_VM_BREAK();
            }
            TX_VM_CASE(CONSTANT_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    CONSTANT_LONG
                );
                static_assert(count == 3);
                push(frame->read_constant<count>());
                TX_VM_BREAK();
            }
            TX_VM_CASE(NIL) : {
                constexpr auto count = get_byte_count_following_opcode(NIL);
                static_assert(count == 0);
                push(Value(val_nil));
                TX_VM_BREAK();
            }
            TX_VM_CASE(TRUE) : {
                constexpr auto count = get_byte_count_following_opcode(TRUE);
                static_assert(count == 0);
                push(Value(true));
                TX_VM_BREAK();
            }
            TX_VM_CASE(FALSE) : {
                constexpr auto count = get_byte_count_following_opcode(FALSE);
                static_assert(count == 0);
                push(Value(false));
                TX_VM_BREAK();
            }
            TX_VM_CASE(POP) : {
                constexpr auto count = get_byte_count_following_opcode(POP);
                static_assert(count == 0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_LOCAL) : {
                constexpr auto count = get_byte_count_following_opcode(GET_LOCAL
                );
                static_assert(count == 1);
                const auto slot = frame->read_multibyte_operand<count>();
                push(frame->slots[slot]);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_LOCAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    GET_LOCAL_LONG
                );
                static_assert(count == 3);
                const auto slot = frame->read_multibyte_operand<count>();
                push(frame->slots[slot]);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL) : {
                constexpr auto count = get_byte_count_following_opcode(SET_LOCAL
                );
                static_assert(count == 1);
                const auto slot = frame->read_multibyte_operand<count>();
                frame->slots[slot] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    SET_LOCAL_LONG
                );
                static_assert(count == 3);
                const auto slot = frame->read_multibyte_operand<count>();
                frame->slots[slot] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    GET_GLOBAL
                );
                static_assert(count == 1);
                auto index = frame->read_multibyte_operand<count>();
                auto value = global_values[index];
                if (value.is_none()) [[unlikely]] {
                    const auto name = get_global_name(index);
                    runtime_error("Undefined variable '{}'.", name);
                    return InterpretResult::RUNTIME_ERROR;
                }
                push(value);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    GET_GLOBAL_LONG
                );
                static_assert(count == 3);
                auto index = frame->read_multibyte_operand<count>();
                auto value = global_values[index];
                if (value.is_none()) [[unlikely]] {
                    const auto name = get_global_name(index);
                    runtime_error("Undefined variable '{}'.", name);
                    return InterpretResult::RUNTIME_ERROR;
                }
                push(value);
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    DEFINE_GLOBAL
                );
                static_assert(count == 1);
                auto index = frame->read_multibyte_operand<count>();
                assert(global_values[index].is_none());
                global_values[index] = peek(0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    DEFINE_GLOBAL_LONG
                );
                static_assert(count == 3);
                auto index = frame->read_multibyte_operand<count>();
                assert(global_values[index].is_none());
                global_values[index] = peek(0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    SET_GLOBAL
                );
                static_assert(count == 1);
                auto index = frame->read_multibyte_operand<count>();
                if (global_values[index].is_none()) [[unlikely]] {
                    const auto name = get_global_name(index);
                    runtime_error("Undefined variable '{}'.", name);
                    return InterpretResult::RUNTIME_ERROR;
                }
                global_values[index] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    SET_GLOBAL_LONG
                );
                static_assert(count == 3);
                auto index = frame->read_multibyte_operand<count>();
                if (global_values[index].is_none()) [[unlikely]] {
                    const auto name = get_global_name(index);
                    runtime_error("Undefined variable '{}'.", name);
                    return InterpretResult::RUNTIME_ERROR;
                }
                global_values[index] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(EQUAL) : {
                constexpr auto count = get_byte_count_following_opcode(EQUAL);
                static_assert(count == 0);
                const Value rhs = pop();
                const Value lhs = pop();
                push(Value(lhs == rhs));
                TX_VM_BREAK();
            }
            TX_VM_CASE(NOT_EQUAL) : {
                constexpr auto count = get_byte_count_following_opcode(NOT_EQUAL
                );
                static_assert(count == 0);
                const Value rhs = pop();
                const Value lhs = pop();
                push(Value(lhs != rhs));
                TX_VM_BREAK();
            }
            TX_VM_CASE(GREATER) : {
                constexpr auto count = get_byte_count_following_opcode(GREATER);
                static_assert(count == 0);
                if (binary_op<std::greater>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(LESS) : {
                constexpr auto count = get_byte_count_following_opcode(LESS);
                static_assert(count == 0);
                if (binary_op<std::less>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(GREATER_EQUAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    GREATER_EQUAL
                );
                static_assert(count == 0);
                if (binary_op<std::greater_equal>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(LESS_EQUAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    LESS_EQUAL
                );
                static_assert(count == 0);
                if (binary_op<std::less_equal>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(ADD) : {
                constexpr auto count = get_byte_count_following_opcode(ADD);
                static_assert(count == 0);
                if (binary_op<std::plus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(SUBSTRACT) : {
                constexpr auto count = get_byte_count_following_opcode(SUBSTRACT
                );
                static_assert(count == 0);
                if (binary_op<std::minus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(MULTIPLY) : {
                constexpr auto count = get_byte_count_following_opcode(MULTIPLY
                );
                static_assert(count == 0);
                if (binary_op<std::multiplies>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(DIVIDE) : {
                constexpr auto count = get_byte_count_following_opcode(DIVIDE);
                static_assert(count == 0);
                if (peek(0).is_int() && peek(1).is_int()
                    && peek(0).as_int() == 0) {
                    runtime_error("Integer division by zero.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                if (binary_op<std::divides>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(NOT) : {
                constexpr auto count = get_byte_count_following_opcode(NOT);
                static_assert(count == 0);
                push(Value(pop().is_falsey()));
                TX_VM_BREAK();
            }
            TX_VM_CASE(NEGATE) : {
                constexpr auto count = get_byte_count_following_opcode(NEGATE);
                static_assert(count == 0);
                if (negate_op()) { return InterpretResult::RUNTIME_ERROR; }
                TX_VM_BREAK();
            }
            TX_VM_CASE(JUMP) : {
                constexpr auto count = get_byte_count_following_opcode(JUMP);
                static_assert(count == 2);
                auto offset = frame->read_multibyte_operand<count>();
                frame->instruction_ptr = std::next(
                    frame->instruction_ptr,
                    offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(JUMP_IF_FALSE) : {
                constexpr auto count = get_byte_count_following_opcode(
                    JUMP_IF_FALSE
                );
                static_assert(count == 2);
                auto offset = frame->read_multibyte_operand<count>();
                frame->instruction_ptr = std::next(
                    frame->instruction_ptr,
                    static_cast<i64>(peek(0).is_falsey()) * offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(LOOP) : {
                constexpr auto count = get_byte_count_following_opcode(LOOP);
                static_assert(count == 2);
                auto offset = frame->read_multibyte_operand<count>();
                frame->instruction_ptr = std::prev(
                    frame->instruction_ptr,
                    offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(CALL) : {
                constexpr auto count = get_byte_count_following_opcode(CALL);
                static_assert(count == 1);
                auto arg_count = frame->read_multibyte_operand<count>();
                if (!call_value(peek(arg_count), arg_count)) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                frame = &frames.back();
                TX_VM_BREAK();
            }
            TX_VM_CASE(END_SCOPE) : {
                constexpr auto count = get_byte_count_following_opcode(END_SCOPE
                );
                static_assert(count == 1);
                const auto slot_count = frame->read_multibyte_operand<count>();
                const auto result = pop();
                for (auto i = 0; i < slot_count; ++i) { pop(); }
                push(result);
                TX_VM_BREAK();
            }
            TX_VM_CASE(END_SCOPE_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    END_SCOPE_LONG
                );
                static_assert(count == 3);
                const auto slot_count = frame->read_multibyte_operand<count>();
                const auto result = pop();
                for (auto i = 0; i < slot_count; ++i) { pop(); }
                push(result);
                TX_VM_BREAK();
            }
            TX_VM_CASE(RETURN) : {
                constexpr auto count = get_byte_count_following_opcode(RETURN);
                static_assert(count == 0);
                const auto result = pop();
                const auto* frame_slots = frame->slots;
                frames.pop_back();
                if (frames.empty()) {
                    pop();
                    assert(stack.empty());
                    return InterpretResult::OK;
                }
                stack.erase(frame_slots, stack.end());
                push(result);
                frame = &frames.back();
                TX_VM_BREAK();
            }
            TX_VM_CASE(END) : { unreachable(); }
        }
    }
    unreachable();
    return InterpretResult::RUNTIME_ERROR;
    // clang-format off
    #undef TX_VM_DISPATCH
    #undef TX_VM_CASE
    #undef TX_VM_BREAK
    // clang-format on
}

}  // namespace tx
