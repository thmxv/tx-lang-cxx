#pragma once

#include "common.hxx"
#include "dyn_array.hxx"
#include "value.hxx"

#include <type_traits>
#include <numeric>

namespace tx {

enum class OpCode : u8 {
    CONSTANT,
    CONSTANT_LONG,
    RETURN,
};

struct ByteCode {
    union {
        OpCode opcode;
        u8 value;
    };

    constexpr ByteCode() noexcept = delete;
    constexpr explicit ByteCode(OpCode opc) noexcept : opcode(opc) {}
    constexpr explicit ByteCode(u8 val) noexcept : value(val) {}

    [[nodiscard]] constexpr OpCode as_opcode() const noexcept { return opcode; }
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
    ValueArray constants;

    void destroy() noexcept {
        code.destroy();
        lines.destroy();
        constants.destroy();
    }

    template <typename T>
        requires std::is_nothrow_constructible_v<ByteCode, T>
    void write(T byte, size_t line) noexcept {
        code.push_back(ByteCode(byte));
        if (lines.size() > 0 && lines[lines.size() - 1].line == line) {
            return;
        }
        lines.push_back(LineStart{.offset = code.size() - 1, .line = line});
    }

    [[nodiscard]] size_t add_constant(Value value) noexcept {
        constants.push_back(value);
        return constants.size() - 1;
    }

    void write_constant(Value value, size_t line) noexcept {
        const u64 index = static_cast<u64>(add_constant(value));
        if (index < (1 << 8)) {
            write(OpCode::CONSTANT, line);
            write(static_cast<u8>(index), line);
            return;
        }
        if (index < (1 << 24)) {
            write(OpCode::CONSTANT_LONG, line);
            write(static_cast<u8>(index & 0xffU), line);
            write(static_cast<u8>((index >> 8U) & 0xffU), line);
            write(static_cast<u8>((index >> 16U) & 0xffU), line);
            return;
        }
        assert(false);
        __builtin_unreachable();
    }

    [[nodiscard]] size_t get_line(size_t instruction) const noexcept {
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
