#pragma once

#include "tx/compiler.hxx"
#include "tx/scanner.hxx"

namespace tx {

inline void compile(VM& /*tvm*/, std::string_view source) {
    Scanner scanner(source);

    int line = -1;
    for (;;) {
        Token token = scanner.scan_token();
        if (token.line != line) {
            fmt::print(FMT_STRING("{:4d} "), token.line);
            line = token.line;
        } else {
            fmt::print("   | ");
        }
        if (token.type == TokenType::INTEGER_LITERAL) {
            fmt::print(
                FMT_STRING("{:2d} '{:s}' {}\n"),
                to_underlying(token.type),
                token.lexeme,
                token.value.as_int
            );
        } else if (token.type == TokenType::FLOAT_LITERAL) {
            fmt::print(
                FMT_STRING("{:2d} '{:s}' {}\n"),
                to_underlying(token.type),
                token.lexeme,
                token.value.as_float
            );
        } else {
            fmt::print(
                FMT_STRING("{:2d} '{:s}'\n"),
                to_underlying(token.type),
                token.lexeme
            );
        }
        if (token.type == TokenType::END_OF_FILE) { break; }
    }
}

}  // namespace tx
