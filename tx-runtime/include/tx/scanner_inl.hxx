#pragma once

#include "tx/fixed_array.hxx"
#include "tx/scanner.hxx"

#include <cassert>
#include <charconv>
#include <cstddef>
#include <iterator>

namespace tx {

[[nodiscard]] constexpr inline bool is_digit(char chr) noexcept {
    return chr >= '0' && chr <= '9';
}

[[nodiscard]] constexpr inline bool is_hex_digit(char chr) noexcept {
    return (chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'F')
           || (chr >= 'a' && chr <= 'f');
}

[[nodiscard]] constexpr inline bool is_alpha(char chr) noexcept {
    return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z')
           || chr == '_';
}

[[nodiscard]] constexpr bool Scanner::is_at_end() const noexcept {
    return current == source.end();
}

constexpr char Scanner::advance() noexcept {
    // 'if' Not needed for null terminated strings
    if (is_at_end()) { return '\0'; }
    auto result = *current;
    current = std::next(current);
    return result;
}

[[nodiscard]] constexpr char Scanner::peek() const noexcept {
    // 'if' Not needed for null terminated strings
    if (is_at_end()) { return '\0'; }
    return *current;
}

[[nodiscard]] constexpr char Scanner::peek_next(size_t offset) const noexcept {
    if (std::distance(current, source.end()) <= offset) { return '\0'; }
    return *std::next(current, offset);
}

[[nodiscard]] constexpr bool Scanner::match(char expected) noexcept {
    if (is_at_end()) { return false; }
    if (*current != expected) { return false; }
    current = std::next(current);
    return true;
}

[[nodiscard]] constexpr Token Scanner::make_token(TokenType type
) const noexcept {
    return Token{
        .type = type,
        .lexeme{start, static_cast<std::size_t>(std::distance(start, current))},
        .line = line
    };
}

[[nodiscard]] constexpr Token Scanner::error_token(std::string_view message
) const noexcept {
    using enum TokenType;
    return Token{.type = ERROR, .lexeme = message, .line = line};
}

constexpr void Scanner::skip_whitespace() noexcept {
    for (;;) {
        const char chr = peek();
        switch (chr) {
            case ' ':
            case '\r':
            case '\t': {
                advance();
                break;
            }
            case '\n': {
                ++line;
                advance();
                break;
            }
            case '#': {
                while (peek() != '\n' && !is_at_end()) { advance(); }
                break;
            }
            default: return;
        }
    }
}

[[nodiscard]] constexpr TokenType Scanner::check_keyword(
    size_t offset,
    std::string_view rest,
    TokenType type
) const noexcept {
    const auto* const sub_start = std::next(start, offset);
    if (rest == std::string_view{sub_start, current}) { return type; }
    return TokenType::IDENTIFIER;
}

[[nodiscard]] constexpr TokenType Scanner::identifier_type() const noexcept {
    switch (start[0]) {
        using enum TokenType;
        case 'a':
            if (std::distance(start, current) > 1) {
                switch (start[1]) {
                    case 'n': return check_keyword(2, "d", AND);
                    case 's':
                        return check_keyword(2, "", AS);
                        if (std::distance(start, current) > 2) {
                            switch (start[2]) {
                                case 'y': return check_keyword(3, "nc", ASYNC);
                            }
                        }
                        break;
                    case 'w': return check_keyword(2, "ait", AWAIT);
                }
            }
            break;
        case 'b': return check_keyword(1, "reak", BREAK);
        case 'c': return check_keyword(1, "ontinue", CONTINUE);
        case 'e': return check_keyword(1, "lse", ELSE);
        case 'f':
            if (std::distance(start, current) > 1) {
                switch (start[1]) {
                    case 'a': return check_keyword(2, "lse", FALSE);
                    case 'o': return check_keyword(2, "r", FOR);
                    case 'n': return check_keyword(2, "", FN);
                }
            }
            break;
        case 'i':
            if (std::distance(start, current) > 1) {
                switch (start[1]) {
                    case 'f': return check_keyword(2, "", IF);
                    case 'm': return check_keyword(2, "port", IMPORT);
                    case 's': return check_keyword(2, "", IS);
                }
            }
            break;
        case 'l':
            if (std::distance(start, current) > 1) {
                switch (start[1]) {
                    case 'e': return check_keyword(2, "t", LET);
                    case 'o': return check_keyword(2, "op", LOOP);
                }
            }
            break;
        case 'n': return check_keyword(1, "il", NIL);
        case 'm': return check_keyword(1, "atch", MATCH);
        case 'o': return check_keyword(1, "r", OR);
        case 'r': return check_keyword(1, "eturn", RETURN);
        case 's':
            if (std::distance(start, current) > 1) {
                switch (start[1]) {
                    case 't': return check_keyword(2, "ruct", STRUCT);
                    case 'u': return check_keyword(2, "per", SUPER);
                }
            }
            break;
        case 't':
            if (std::distance(start, current) > 1) {
                switch (start[1]) {
                    case 'h': return check_keyword(2, "is", THIS);
                    case 'r': return check_keyword(2, "ue", TRUE);
                }
            }
            break;
        case 'v': return check_keyword(1, "ar", VAR);
        case 'w': return check_keyword(1, "hile", WHILE);
        case 'A': return check_keyword(1, "ny", ANY);
        case 'B': return check_keyword(1, "ool", BOOL);
        case 'C': return check_keyword(1, "har", CHAR);
        case 'F': return check_keyword(1, "loat", FLOAT);
        case 'I': return check_keyword(1, "nt", INT);
        case 'N': return check_keyword(1, "il", NIL_TYPE);
        case 'S': return check_keyword(1, "tr", STR);
    }
    return TokenType::IDENTIFIER;
}

[[nodiscard]] constexpr Token Scanner::identifier() noexcept {
    while (is_alpha(peek()) || is_digit(peek())) { advance(); }
    return make_token(identifier_type());
}

[[nodiscard]] Token Scanner::number() noexcept {
    TokenType type = TokenType::INTEGER_LITERAL;
    while (is_digit(peek()) || peek() == '_') { advance(); }
    if (peek() == '.' && (is_digit(peek_next()) || peek_next() == '_')) {
        advance();
        while (is_digit(peek()) || peek() == '_') { advance(); }
        type = TokenType::FLOAT_LITERAL;
    }
    if (match('e') || match('E')) {
        if (!match('+')) { (void)match('-'); }
        if (!is_digit(peek()) && peek() != '_') {
            return error_token("Unterminated scientific notation.");
        }
        while (is_digit(peek()) || peek() == '_') { advance(); }
        type = TokenType::FLOAT_LITERAL;
    }
    if (std::distance(start, current)
        > static_cast<std::ptrdiff_t>(MAX_CHARS_IN_NUMERIC_LITERAL)) {
        return error_token("Numeric literal too long.");
    }
    FixedCapacityArray<char, size_t, MAX_CHARS_IN_NUMERIC_LITERAL> no_space{};
    std::copy_if(start, current, std::back_inserter(no_space), [](char chr) {
        return chr != '_';
    });
    switch (type) {
        using enum TokenType;
        case INTEGER_LITERAL: {
            int_t value = 0;
            [[maybe_unused]] auto fcr = std::from_chars(
                no_space.cbegin(),
                no_space.cend(),
                value
            );
            assert(fcr.ptr == no_space.cend());
            if (fcr.ec == std::errc::result_out_of_range) {
                return error_token("Numeric literal out of range.");
            }
            auto token = make_token(type);
            token.value.as_int = value;
            return token;
        }
        case FLOAT_LITERAL: {
            float_t value = 0.0;
            [[maybe_unused]] auto fcr = std::from_chars(
                no_space.cbegin(),
                no_space.cend(),
                value
            );
            assert(fcr.ptr == no_space.cend());
            if (fcr.ec == std::errc::result_out_of_range) {
                return error_token("Numeric literal out of range.");
            }
            auto token = make_token(type);
            token.value.as_float = value;
            return token;
        }
        default: unreachable();
    }
}

[[nodiscard]] inline Token Scanner::hex_number() noexcept {
    advance();
    while (is_hex_digit(peek()) || peek() == '_') { advance(); }
    if (std::distance(start, current)
        > static_cast<std::ptrdiff_t>(MAX_CHARS_IN_NUMERIC_LITERAL)) {
        return error_token("Hexadecimal integer literal too long.");
    }
    FixedCapacityArray<char, size_t, MAX_CHARS_IN_NUMERIC_LITERAL> no_space{};
    std::copy_if(start, current, std::back_inserter(no_space), [](char chr) {
        return chr != '_';
    });
    int_t value = 0;
    [[maybe_unused]] auto fcr = std::from_chars(
        std::next(no_space.cbegin(), 2),
        no_space.cend(),
        value,
        16
    );
    assert(fcr.ptr == no_space.cend());
    if (fcr.ec == std::errc::result_out_of_range) {
        return error_token("Hexadecimal integer literal out of range.");
    }
    auto token = make_token(TokenType::INTEGER_LITERAL);
    token.value.as_int = value;
    return token;
}

[[nodiscard]] constexpr Token Scanner::raw_string() noexcept {
    advance();
    advance();
    while ((peek() != '"' || peek_next() != '"' || peek_next(2) != '"')
           && !is_at_end()) {
        if (peek() == '\n') { ++line; }
        advance();
    }
    if (is_at_end()) { return error_token("Unterminated raw string."); }
    advance();
    advance();
    advance();
    return make_token(TokenType::STRING_LITERAL);
}

[[nodiscard]] constexpr Token Scanner::string() noexcept {
    FixedCapacityArray<char, size_t, 1024> string;
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '$') {
            if (str_interp_braces.size() < MAX_INTERP_DEPTH) {
                if (peek_next() != '{') {
                    return error_token("Expect '{' after '$'.");
                }
                str_interp_braces.push_back(1);
                auto token = make_token(TokenType::STRING_INTERPOLATION);
                advance();
                advance();
                fmt::print(
                    "'{}'\n",
                    std::string_view{
                        string.data(),
                        static_cast<std::size_t>(string.size())}
                );
                return token;
            }
            return error_token("Nested string interpolation too deep.");
        }
        if (peek() == '\n') { ++line; }
        if (peek() == '\\') {
            advance();
            switch (peek()) {
                case '"': string.push_back('"'); break;
                case '\\': string.push_back('\\'); break;
                case '$': string.push_back('$'); break;
                case '0': string.push_back('\0'); break;
                case 'a': string.push_back('\a'); break;
                case 'b': string.push_back('\b'); break;
                case 'e': string.push_back('\33'); break;
                case 'f': string.push_back('\f'); break;
                case 'n': string.push_back('\n'); break;
                case 'r': string.push_back('\r'); break;
                case 't': string.push_back('\t'); break;
                // case 'u': readUnicodeEscape(parser, &string, 4); break;
                // case 'U': readUnicodeEscape(parser, &string, 8); break;
                case 'v': string.push_back('\v'); break;
                // case 'x': string.push_back(
                //   (uint8_t)readHexEscape(parser, 2, "byte")
                // ); break;
                default: return error_token("Invalid escape character.");
            }
            advance();
        } else {
            string.push_back(advance());
        }
    }
    if (is_at_end()) { return error_token("Unterminated string."); }
    advance();
    fmt::print(
        "'{}'\n",
        std::string_view{string.data(), static_cast<std::size_t>(string.size())}
    );
    return make_token(TokenType::STRING_LITERAL);
}

[[nodiscard]] constexpr Token Scanner::scan_token() noexcept {
    skip_whitespace();
    start = current;
    using enum TokenType;
    if (is_at_end()) { return make_token(END_OF_FILE); }
    const char chr = advance();
    switch (chr) {
        case '(': return make_token(LEFT_PAREN);
        case ')': return make_token(RIGHT_PAREN);
        case '{':
            // If we are inside an interpolated expression, count the "{"
            if (!str_interp_braces.empty()) { str_interp_braces.back()++; }
            return make_token(LEFT_BRACE);
        case '}':
            // If we are inside an interpolated expression, count the "}"
            if (!str_interp_braces.empty() && --str_interp_braces.back() == 0) {
                str_interp_braces.pop_back();
                return string();
            }
            return make_token(RIGHT_BRACE);
        case '[': return make_token(LEFT_BRACKET);
        case ']': return make_token(RIGHT_BRACKET);
        case ':': return make_token(COLON);
        case ';': return make_token(SEMICOLON);
        case ',': return make_token(COMMA);
        case '.': return make_token(DOT);
        case '-': return make_token(MINUS);
        case '|': return make_token(PIPE);
        case '+': return make_token(PLUS);
        case '/': return make_token(SLASH);
        case '*': return make_token(STAR);
        case '!': return make_token(match('=') ? BANG_EQUAL : BANG);
        case '=': return make_token(match('=') ? EQUAL_EQUAL : EQUAL);
        case '<': return make_token(match('=') ? LESS_EQUAL : LEFT_CHEVRON);
        case '>': return make_token(match('=') ? GREATER_EQUAL : RIGHT_CHEVRON);
        case '"': {
            if (peek() == '"' && peek_next() == '"') { return raw_string(); }
            return string();
        }
        case '0':
            if (peek() == 'x' || peek() == 'X') { return hex_number(); }
            return number();
        default:
            if (is_alpha(chr)) { return identifier(); }
            if (is_digit(chr)) { return number(); }
            return error_token("Unexpected character.");
    }
}

}  // namespace tx
