#pragma once

#include "tx/common.hxx"
#include "tx/dyn_array.hxx"
#include "tx/value.hxx"

#include <iterator>
#include <numeric>
#include <utility>
#include <type_traits>

namespace tx {

// clang-format off
enum class OpCode : u8 {
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
    #define TX_OPCODE(name, length, _) name,
    #include "tx/opcodes.inc"
    #undef TX_OPCODE
};

inline constexpr std::array opcode_length_table = {
    // NOLINTNEXTLINE(*-macro-usage)
    #define TX_OPCODE(name, length, _) length,
    #include "tx/opcodes.inc"
    #undef TX_OPCODE
};
// clang-format on

static inline constexpr size_t get_byte_count_following_opcode(OpCode opc
) noexcept {
    return opcode_length_table[to_underlying(opc)];
}

struct ByteCode {
    union {
        OpCode opcode;
        u8 value;
    };

    constexpr ByteCode() noexcept = delete;
    // NOLINTNEXTLINE(*-member-init)
    constexpr explicit ByteCode(OpCode opc) noexcept : opcode(opc) {}
    // NOLINTNEXTLINE(*-member-init)
    constexpr explicit ByteCode(u8 val) noexcept : value(val) {}

    // NOLINTNEXTLINE(*-union-access)
    [[nodiscard]] constexpr OpCode as_opcode() const noexcept { return opcode; }
    // NOLINTNEXTLINE(*-union-access)
    [[nodiscard]] constexpr u8 as_u8() const noexcept { return value; }
};

struct LineStart {
    size_t offset;
    size_t line;
};

template <u32 N>
constexpr inline void
write_multibyte_operand(ByteCode*& ptr, size_t value) noexcept {
    static_assert(N <= 3);
    const auto val = static_cast<u32>(value);
    for (u32 i = 0; i < N; ++i) {
        (*std::next(ptr, i)).value = static_cast<u8>((val >> (i * 8U)) & 0xffU);
    }
}

template <u32 N>
constexpr inline size_t read_multibyte_operand(const ByteCode* ptr) noexcept {
    static_assert(N <= 3);
    u32 result = 0;
    for (u32 i = 0; i < N; ++i) {
        result += static_cast<u32>(std::next(ptr, i)->as_u8()) << (i * 8U);
    }
    return size_cast(result);
}

constexpr inline std::tuple<bool, size_t, u8> read_closure_operand(
    const ByteCode* ptr
) noexcept {
    u8 flags = ptr->as_u8();
    std::advance(ptr, 1);
    const u8 length = flags & 0b01111111U;
    const bool is_local = (flags & 0b10000000U) != 0U;
    assert(length >= 1);
    assert(length <= 3);
    auto index = [&]() {
        if (length == 1) { return read_multibyte_operand<1>(ptr); }
        if (length == 2) { return read_multibyte_operand<2>(ptr); }
        if (length == 3) { return read_multibyte_operand<3>(ptr); }
        unreachable();
    }();
    return std::make_tuple(is_local, index, 1 + length);
}

using ByteCodeArray = DynArray<ByteCode, size_t>;
using LineStartArray = DynArray<LineStart, size_t>;

struct Chunk {
    ByteCodeArray code;
    LineStartArray lines;
    // ConstValueArray constants;
    ValueArray constants;

    constexpr void destroy(VM& tvm) noexcept {
        code.destroy(tvm);
        lines.destroy(tvm);
        constants.destroy(tvm);
    }

    constexpr void write_line(VM& tvm, size_t line) {
        if (lines.size() > 0 && lines[lines.size() - 1].line == line) {
            return;
        }
        lines.push_back(tvm, LineStart{.offset = code.size(), .line = line});
    }

    [[nodiscard]] constexpr size_t add_constant(VM& tvm, Value value) noexcept {
        constants.push_back(tvm, value);
        return constants.size() - 1;
    }

    template <typename... Ts>
        requires((std::is_nothrow_constructible_v<ByteCode, Ts>) && ...)
    constexpr void write_bytes(VM& tvm, size_t line, Ts... bytes) noexcept {
        write_line(tvm, line);
        (code.push_back(tvm, ByteCode(bytes)), ...);
    }

    template <u32 N>
    constexpr inline void
    write_multibyte_operand(VM& tvm, size_t line, size_t operand) noexcept {
        static_assert(N <= 3);
        write_line(tvm, line);
        auto offset = code.size();
        code.resize(tvm, code.size() + size_cast(N), ByteCode(OpCode::END));
        auto* ptr = std::next(code.begin(), offset);
        ::tx::write_multibyte_operand<N>(ptr, operand);
    }

    template <u32 N>
    constexpr inline void write_instruction(
        VM& tvm,
        size_t line,
        OpCode opc,
        size_t operand
    ) noexcept {
        static_assert(N <= 3);
        write_line(tvm, line);
        code.push_back(tvm, ByteCode(opc));
        write_multibyte_operand<N>(tvm, line, operand);
        // auto offset = code.size();
        // code.resize(tvm, code.size() + size_cast(N), ByteCode(OpCode::END));
        // auto* ptr = std::next(code.begin(), offset);
        // ::tx::write_multibyte_operand<N>(ptr, operand);
    }

    [[nodiscard]] constexpr size_t get_line(size_t instruction) const noexcept {
        size_t start = 0;
        size_t end = lines.size() - 1;
        while (true) {
            size_t const mid = std::midpoint(start, end);
            const auto& line = lines[mid];
            if (instruction < line.offset) {
                end = mid - 1;
            } else if ( //
		mid == lines.size() - 1
		|| instruction < lines[mid + 1].offset
	    ) {
                return line.line;
            } else {
                start = mid + 1;
            }
        }
    }
};

}  // namespace tx
