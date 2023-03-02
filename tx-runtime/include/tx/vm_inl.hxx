#pragma once

#include "tx/type.hxx"
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
#include <tuple>
#include <type_traits>

namespace tx {

inline NativeResult core_version_string_native(VM& tvm, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    (void)args;
    inout.return_value() = Value{make_string(tvm, false, VERSION)};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
core_version_major_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    (void)args;
    inout.return_value() = Value{static_cast<int_t>(VERSION_MAJOR)};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
core_version_minor_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    (void)args;
    inout.return_value() = Value{static_cast<int_t>(VERSION_MINOR)};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
core_version_patch_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    (void)args;
    inout.return_value() = Value{static_cast<int_t>(VERSION_PATCH)};
    return NativeResult::SUCCESS;
}

inline constexpr NativeResult
core_version_tweak_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    (void)args;
    inout.return_value() = Value{static_cast<int_t>(VERSION_TWEAK)};
    return NativeResult::SUCCESS;
}

inline NativeResult core_assert_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.size() == 2);
    assert(args[1].is_object() && args[1].as_object().is_string());
    if (args[0].is_falsey()) [[unlikely]] {
        inout.return_value() = Value{args[1]};
        return NativeResult::RUNTIME_ERROR;
    }
    inout.return_value() = Value{val_nil};
    return NativeResult::SUCCESS;
}

inline NativeResult std_cpu_clock_read_native(VM& /*tvm*/, NativeInOut inout) {
    const auto args = inout.args();
    assert(args.empty());
    (void)args;
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
    (void)args;
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
    fmt::print(FMT_STRING("{}\n"), args[0]);
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
    // if (std::isnan(res)) [[unlikely]] { std::feclearexcept(FE_INVALID); }
    inout.return_value() = Value{res};
    return NativeResult::SUCCESS;
}

[[nodiscard]] inline constexpr ByteCode CallFrame::read_byte() noexcept {
    auto result = *instruction_ptr;
    instruction_ptr = std::next(instruction_ptr);
    return result;
}

[[nodiscard]] inline constexpr std::tuple<bool, size_t>
CallFrame::read_closure_operand() noexcept {
    auto [is_local, index, len] = ::tx::read_closure_operand(instruction_ptr);
    std::advance(instruction_ptr, len);
    return std::make_tuple(is_local, index);
}

inline VM::VM(VMOptions opts, const Allocator& alloc) noexcept
        : options(opts)
        , allocator(alloc) {
    if constexpr (!IS_DEBUG_BUILD) {
        frames.reserve(*this, FRAMES_START);
        stack.reserve(*this, STACK_START);
    }

    // FIXME: detect signature automatically at compile time
    define_native(
        "core_version_string",
        core_version_string_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types = {},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::STRING}}},
         }}}}
    );
    define_native(
        "core_version_major",
        core_version_major_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types = {},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::INT}}},
         }}}}
    );
    define_native(
        "core_version_minor",
        core_version_minor_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types = {},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::INT}}},
         }}}}
    );
    define_native(
        "core_version_patch",
        core_version_patch_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types = {},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::INT}}},
         }}}}
    );
    define_native(
        "core_version_tweak",
        core_version_tweak_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types = {},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::INT}}},
         }}}}
    );
    define_native(
        "core_assert",
        core_assert_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types =
                 {*this,
                  {
                      TypeSet{*this, {TypeInfo{TypeInfo::Type::ANY}}},
                      TypeSet{*this, {TypeInfo{TypeInfo::Type::STRING}}},
                  }},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::NIL}}},
         }}}}
    );
    define_native(
        "std_cpu_clock_read",
        std_cpu_clock_read_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types = {},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::INT}}},
         }}}}
    );
    define_native(
        "std_cpu_clock_elapsed",
        std_cpu_clock_elapsed_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types =
                 {*this, {TypeSet{*this, {TypeInfo{TypeInfo::Type::INT}}}}},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::FLOAT}}},
         }}}}
    );
    define_native(
        "std_wall_clock_read",
        std_wall_clock_read_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types = {},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::INT}}},
         }}}}
    );
    define_native(
        "std_wall_clock_elapsed",
        std_wall_clock_elapsed_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types =
                 {*this, {TypeSet{*this, {TypeInfo{TypeInfo::Type::INT}}}}},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::FLOAT}}},
         }}}}
    );
    define_native(
        "std_sleep_for",
        std_sleep_for_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types =
                 {*this,
                  {TypeSet{
                      *this,
                      {TypeInfo{TypeInfo::Type::INT},
                       TypeInfo{TypeInfo::Type::FLOAT}}}}},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::NIL}}},
         }}}}
    );
    define_native(
        "std_println",
        std_println_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             // TODO: Force Str parameter, not Any
             .parameter_types =
                 {*this, {{*this, {TypeInfo{TypeInfo::Type::ANY}}}}},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::NIL}}},
         }}}}
    );
    define_native(
        "Float_has_integer_value",
        float_has_integer_value_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types =
                 {*this, {{*this, {TypeInfo{TypeInfo::Type::FLOAT}}}}},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::BOOL}}},
         }}}}
    );
    define_native(
        "Float_sqrt",
        float_sqrt_native,
        {*this,
         {TypeInfo{TypeInfoFunction{
             .parameter_types =
                 {*this, {{*this, {TypeInfo{TypeInfo::Type::FLOAT}}}}},
             .return_type = {*this, {TypeInfo{TypeInfo::Type::FLOAT}}},
         }}}}
    );
}

inline constexpr VM::~VM() noexcept {
    stack.destroy(*this);
    frames.destroy(*this);
    global_indices.destroy(*this);
    global_values.destroy(*this);
    for (auto& global : global_signatures) { global.destroy(*this); }
    global_signatures.destroy(*this);
    strings.destroy(*this);
    free_objects(*this, objects);
    gray_stack.destroy(*this);
}

inline constexpr void VM::reset_stack() noexcept {
    stack.clear();
    frames.clear();
    open_upvalues = nullptr;
    if constexpr (IS_DEBUG_BUILD) {
        stack.destroy(*this);
        frames.destroy(*this);
    }
}

inline void VM::runtime_error_impl() noexcept {
    fmt::print(stderr, FMT_STRING("\n"));
    for (size_t i = frames.size() - 1; i >= 0; --i) {
        const auto& frame = frames[i];
        const auto& function = frame.closure.function;
        auto instruction = frame.instruction_ptr - function.chunk.code.begin()
                           - 1;
        fmt::print(
            stderr,
            FMT_STRING("  [{:s}:{:d}] in {:s}\n"),
            function.module_file_path,
            function.chunk.get_line(static_cast<i32>(instruction)),
            function.get_display_name()
        );
    }
    reset_stack();
}

inline ObjFunction*
VM::compile(std::string_view file_path, std::string_view source) noexcept {
    Parser current_parser(*this);
    parser = &current_parser;
    ensure_stack_space(1);
    ObjFunction* function = parser->compile(file_path, source);
    parser = nullptr;
    return function;
}

// TX_VM_CONSTEXPR
inline InterpretResult
VM::interpret(std::string_view file_path, std::string_view source) noexcept {
    ObjFunction* function = compile(file_path, source);
    if (function == nullptr) { return InterpretResult::COMPILE_ERROR; }
    push(Value{function});
    auto* closure = make_closure(*this, *function);
    pop();
    push(Value{closure});
    (void)call(*closure, 0);
    auto result = run();
    assert(stack.empty());
    return result;
}

inline constexpr size_t
VM::add_global(Value name, Global&& signature, Value val) noexcept {
    // NOTE: maybe do the GC push/pop dance here
    auto new_index = global_values.size();
    global_values.push_back(*this, val);
    global_signatures.push_back(*this, std::move(signature));
    global_indices.set(*this, name, Value{static_cast<int_t>(new_index)});
    return new_index;
}

[[nodiscard]] inline constexpr std::string_view VM::get_global_name(size_t index
) const noexcept {
    assert(index < global_values.size());
    for (const auto& entry : global_indices) {
        // cppcheck-suppress useStlAlgorithm
        if (entry.second.as_int() == index) {
            return entry.first.as_object().as<ObjString>();
        }
    }
    unreachable();
}

inline void VM::define_native(
    std::string_view name,
    NativeFn fun,
    TypeSet&& type_set
) noexcept {
    assert(stack.empty());
    ensure_stack_space(2);
    push(Value{make_string(*this, false, name)});
    push(Value{allocate_object<ObjNative>(*this, fun)});
    add_global(
        stack[0],
        Global{
            .is_defined = true,
            .is_const = true,
            .type_set = std::move(type_set),
        },
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
        for (auto* upvalue = open_upvalues; upvalue != nullptr;
             upvalue = upvalue->next_upvalue) {
            upvalue->location = std::next(
                stack.begin(),
                std::distance(old, upvalue->location)
            );
        }
    }
}

[[nodiscard]] inline constexpr bool
VM::call(ObjClosure& closure, size_t arg_c) noexcept {
    // TODO: make this a compile time error
    if (arg_c != closure.function.arity) [[unlikely]] {
        runtime_error(
            "Expected {:d} arguments but got {:d}.",
            closure.function.arity,
            arg_c
        );
        return false;
    }
    if (frames.size() == FRAMES_MAX) [[unlikely]] {
        runtime_error("Stack overflow.");
        return false;
    }
    const auto stack_needed = stack.size() + closure.function.max_slots;
    ensure_stack_space(stack_needed);
    frames.emplace_back(
        *this,
        closure,
        closure.function.chunk.code.begin(),
        std::prev(stack.end(), arg_c + 1)
    );
    return true;
}

[[nodiscard]] inline constexpr bool
VM::call_value(Value callee, size_t arg_c) noexcept {
    if (callee.is_object()) [[likely]] {
        auto& obj = callee.as_object();
        switch (obj.type) {
            using enum Obj::ObjType;
            case CLOSURE: return call(obj.as<ObjClosure>(), arg_c);
            case NATIVE: {
                auto& native = obj.as<ObjNative>();
                auto success = std::invoke(
                    native.function,
                    *this,
                    NativeInOut(std::prev(stack.end(), arg_c + 1), arg_c + 1)
                );
                if (success == NativeResult::SUCCESS) [[likely]] {
                    // Return value already put in right slot by native
                    // func. Do not erase it :)
                    stack.erase(std::prev(stack.cend(), arg_c), stack.cend());
                    return true;
                }
                const auto& return_value = *std::prev(stack.cend(), arg_c + 1);
                runtime_error(
                    FMT_STRING("{:s}"),
                    return_value.as_object().as<ObjString>()
                );
                return false;
            }
                // clang-format off
            [[unlikely]] default : break;
                // clang-format on
        }
    }
    // TODO: make this a compile time error
    runtime_error("Can only call functions.");
    return false;
}

[[nodiscard]] inline ObjUpvalue& VM::capture_upvalue(Value* local) noexcept {
    ObjUpvalue* prev_upvalue = nullptr;
    ObjUpvalue* upvalue = open_upvalues;
    while (upvalue != nullptr && upvalue->location > local) {
        prev_upvalue = upvalue;
        upvalue = upvalue->next_upvalue;
    }
    if (upvalue != nullptr && upvalue->location == local) { return *upvalue; }
    auto* created_upvalue = allocate_object<ObjUpvalue>(*this, local);
    created_upvalue->next_upvalue = upvalue;
    if (prev_upvalue == nullptr) {
        open_upvalues = created_upvalue;
    } else {
        prev_upvalue->next_upvalue = created_upvalue;
    }
    return *created_upvalue;
}

inline constexpr void VM::close_upvalues(const Value* last) noexcept {
    while (open_upvalues != nullptr && open_upvalues->location >= last) {
        ObjUpvalue* upvalue = open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        open_upvalues = upvalue->next_upvalue;
    }
}

[[nodiscard]] inline constexpr bool VM::negate_op() noexcept {
    Value result{val_none};
    if (peek(0).is_int()) {
        result = Value(-pop().as_int());
    } else if (peek(0).is_float()) {
        result = Value(-pop().as_float());
    } else [[unlikely]] {
        // TODO: make this a compile time error
        runtime_error("Operand must be a number.");
        return true;
    }
    push(result);
    return false;
}

template <template <typename> typename Op>
[[nodiscard]] inline constexpr bool VM::binary_op() noexcept {
    if (!peek(0).is_number() || !peek(1).is_number()) [[unlikely]] {
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
    disassemble_instruction(closure.function.chunk, instruction_ptr);
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
            (void)previous_operand;
            (void)delta;
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

template <u8 N>
inline void VM::do_constant(CallFrame*& frame) noexcept {
    push(frame->read_constant<N>());
}

template <u8 N>
inline void VM::do_get_local(CallFrame*& frame) noexcept {
    const auto slot = frame->read_multibyte_operand<N>();
    push(frame->slots[slot]);
}

template <u8 N>
inline void VM::do_set_local(CallFrame*& frame) noexcept {
    const auto slot = frame->read_multibyte_operand<N>();
    frame->slots[slot] = peek(0);
}

template <u8 N>
inline bool VM::do_get_global(CallFrame*& frame) noexcept {
    auto index = frame->read_multibyte_operand<N>();
    auto value = global_values[index];
    if (value.is_none()) [[unlikely]] {
        const auto name = get_global_name(index);
        runtime_error("Undefined variable '{}'.", name);
        return true;
    }
    push(value);
    return false;
}

template <u8 N>
inline void VM::do_define_global(CallFrame*& frame) noexcept {
    auto index = frame->read_multibyte_operand<N>();
    if (!options.allow_global_redefinition) {
        assert(global_values[index].is_none());
    }
    global_values[index] = peek(0);
    pop();
}

template <u8 N>
inline bool VM::do_set_global(CallFrame*& frame) noexcept {
    auto index = frame->read_multibyte_operand<N>();
    if (global_values[index].is_none()) [[unlikely]] {
        const auto name = get_global_name(index);
        runtime_error("Undefined variable '{}'.", name);
        return true;
    }
    global_values[index] = peek(0);
    return false;
}

template <u8 N>
inline void VM::do_get_upvalue(CallFrame*& frame) noexcept {
    const auto slot = frame->read_multibyte_operand<N>();
    push(*frame->closure.upvalues[slot]->location);
}

template <u8 N>
inline void VM::do_set_upvalue(CallFrame*& frame) noexcept {
    const auto slot = frame->read_multibyte_operand<N>();
    *frame->closure.upvalues[slot]->location = peek(0);
}

template <u8 N>
inline void VM::do_closure(CallFrame*& frame) noexcept {
    auto& fun = frame->read_constant<N>().as_object().template as<ObjFunction>(
    );
    auto* closure = make_closure(*this, fun);
    push(Value(closure));
    for (auto& upvalue : closure->upvalues) {
        auto [is_local, index] = frame->read_closure_operand();
        if (is_local) {
            upvalue = &capture_upvalue(std::next(frame->slots, index));
        } else {
            upvalue = frame->closure.upvalues[index];
        }
    }
}

template <u8 N>
inline void VM::do_end_scope(CallFrame*& frame) noexcept {
    const auto slot_count = frame->read_multibyte_operand<N>();
    const auto result = pop();
    close_upvalues(std::prev(stack.end(), slot_count));
    for (auto i = 0; i < slot_count; ++i) { pop(); }
    push(result);
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
                do_constant<1>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(CONSTANT_LONG) : {
                do_constant<3>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(NIL) : {
                push(Value(val_nil));
                TX_VM_BREAK();
            }
            TX_VM_CASE(TRUE) : {
                push(Value(true));
                TX_VM_BREAK();
            }
            TX_VM_CASE(FALSE) : {
                push(Value(false));
                TX_VM_BREAK();
            }
            TX_VM_CASE(POP) : {
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_LOCAL) : {
                do_get_local<1>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_LOCAL_LONG) : {
                do_get_local<3>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL) : {
                do_set_local<1>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL_LONG) : {
                do_set_local<3>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL) : {
                if (do_get_global<1>(frame)) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL_LONG) : {
                if (do_get_global<3>(frame)) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL) : {
                do_define_global<1>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL_LONG) : {
                do_define_global<3>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL) : {
                if (do_set_global<1>(frame)) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL_LONG) : {
                if (do_set_global<3>(frame)) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_UPVALUE) : {
                do_get_upvalue<1>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_UPVALUE_LONG) : {
                do_get_upvalue<3>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_UPVALUE) : {
                do_set_upvalue<1>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_UPVALUE_LONG) : {
                do_set_upvalue<3>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(EQUAL) : {
                const Value rhs = pop();
                const Value lhs = pop();
                push(Value(lhs == rhs));
                TX_VM_BREAK();
            }
            TX_VM_CASE(NOT_EQUAL) : {
                const Value rhs = pop();
                const Value lhs = pop();
                push(Value(lhs != rhs));
                TX_VM_BREAK();
            }
            TX_VM_CASE(GREATER) : {
                if (binary_op<std::greater>()) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(LESS) : {
                if (binary_op<std::less>()) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(GREATER_EQUAL) : {
                if (binary_op<std::greater_equal>()) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(LESS_EQUAL) : {
                if (binary_op<std::less_equal>()) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(ADD) : {
                if (binary_op<std::plus>()) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(SUBSTRACT) : {
                if (binary_op<std::minus>()) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(MULTIPLY) : {
                if (binary_op<std::multiplies>()) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(DIVIDE) : {
                if (peek(0).is_int() && peek(1).is_int()
                    && peek(0).as_int() == 0) [[unlikely]] {
                    runtime_error("Integer division by zero.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                if (binary_op<std::divides>()) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(NOT) : {
                push(Value(pop().is_falsey()));
                TX_VM_BREAK();
            }
            TX_VM_CASE(NEGATE) : {
                if (negate_op()) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(JUMP) : {
                auto offset = frame->read_multibyte_operand<2>();
                frame->instruction_ptr = std::next(
                    frame->instruction_ptr,
                    offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(JUMP_IF_FALSE) : {
                auto offset = frame->read_multibyte_operand<2>();
                frame->instruction_ptr = std::next(
                    frame->instruction_ptr,
                    static_cast<i64>(peek(0).is_falsey()) * offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(LOOP) : {
                auto offset = frame->read_multibyte_operand<2>();
                frame->instruction_ptr = std::prev(
                    frame->instruction_ptr,
                    offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(CALL) : {
                auto arg_count = frame->read_multibyte_operand<1>();
                if (!call_value(peek(arg_count), arg_count)) [[unlikely]] {
                    return InterpretResult::RUNTIME_ERROR;
                }
                frame = &frames.back();
                TX_VM_BREAK();
            }
            TX_VM_CASE(CLOSURE) : {
                do_closure<1>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(CLOSURE_LONG) : {
                do_closure<3>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(END_SCOPE) : {
                do_end_scope<1>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(END_SCOPE_LONG) : {
                do_end_scope<3>(frame);
                TX_VM_BREAK();
            }
            TX_VM_CASE(RETURN) : {
                const auto result = pop();
                const auto* frame_slots = frame->slots;
                close_upvalues(frame_slots);
                frames.pop_back();
                if (frames.empty()) [[unlikely]] {
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
