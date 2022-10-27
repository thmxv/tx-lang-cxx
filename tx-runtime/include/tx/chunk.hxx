#pragma once

#include "tx/common.hxx"
#include "tx/dyn_array.hxx"
#include "tx/value.hxx"

#include <numeric>
#include <utility>
#include <type_traits>

namespace tx {

// clang-format off
enum class OpCode : u8 {
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
    #define TX_OPCODE(name, _) name,
    #include "tx/opcodes.inc"
    #undef TX_OPCODE
};
// clang-format on

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

    template <typename... Ts>
        requires((std::is_nothrow_constructible_v<ByteCode, Ts>) && ...)
    constexpr void write(VM& tvm, size_t line, Ts... bytes) noexcept {
        write_line(tvm, line);
        (code.push_back(tvm, ByteCode(bytes)), ...);
    }

    [[nodiscard]] constexpr size_t add_constant(VM& tvm, Value value) noexcept {
        constants.push_back(tvm, value);
        return constants.size() - 1;
    }

    constexpr void write_constant(
        VM& tvm,
        size_t line,
        OpCode opc,
        size_t constant_index
    ) noexcept {
        const u64 index = static_cast<u64>(constant_index);
        if (index < (1U << 8U)) {  // NOLINT(*-magic-numbers)
            write(tvm, line, opc, static_cast<u8>(index));
            return;
        }
        if (index < (1U << 24U)) {  // NOLINT(*-magic-numbers)
            opc = OpCode(to_underlying(opc) + 1);
            write(
                tvm,
                line,
                opc,
                // NOLINTNEXTLINE(*-magic-numbers)
                static_cast<u8>(index & 0xffU),
                // NOLINTNEXTLINE(*-magic-numbers)
                static_cast<u8>((index >> 8U) & 0xffU),
                // NOLINTNEXTLINE(*-magic-numbers)
                static_cast<u8>((index >> 16U) & 0xffU)
            );
            return;
        }
        unreachable();
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

constexpr inline std::pair<size_t, const ByteCode*>
read_constant_index(const ByteCode* ptr, bool is_long) noexcept {
    if (!is_long) { return std::make_pair(ptr->as_u8(), std::next(ptr, 1)); }
    const auto constant_idx =  //
        static_cast<u32>((ptr)->as_u8())
        | (static_cast<u32>((std::next(ptr, 1))->as_u8()) << 8U)
        | (static_cast<u32>((std::next(ptr, 2))->as_u8()) << 16U);
    return std::make_pair(static_cast<size_t>(constant_idx), std::next(ptr, 3));
}

}  // namespace tx
