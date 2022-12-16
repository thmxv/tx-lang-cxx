#pragma once

#include "tx/scanner.hxx"
//
#include "tx/fixed_array.hxx"
#include "tx/object.hxx"
#include "tx/unicode.hxx"
#include "tx/utils.hxx"
#include "tx/vm.hxx"

#include <cassert>
#include <charconv>
#include <cstddef>
#include <iterator>

namespace tx {

[[nodiscard]] inline constexpr bool is_digit(char chr) noexcept {
    return chr >= '0' && chr <= '9';
}

[[nodiscard]] inline constexpr bool is_hex_digit(char chr) noexcept {
    return (chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'F')
           || (chr >= 'a' && chr <= 'f');
}

[[nodiscard]] inline constexpr bool is_alpha(char chr) noexcept {
    return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z')
           || chr == '_';
}

[[nodiscard]] inline constexpr bool Scanner::is_at_end() const noexcept {
    return current == source.end();
}

inline constexpr char Scanner::advance() noexcept {
    // 'if' Not needed for null terminated strings
    if (is_at_end()) { return '\0'; }
    auto result = *current;
    current = std::next(current);
    return result;
}

[[nodiscard]] inline constexpr char Scanner::peek() const noexcept {
    // 'if' Not needed for null terminated strings
    if (is_at_end()) { return '\0'; }
    return *current;
}

[[nodiscard]] inline constexpr char Scanner::peek_next(size_t offset
) const noexcept {
    if (std::distance(current, source.end()) <= offset) { return '\0'; }
    return *std::next(current, offset);
}

[[nodiscard]] inline constexpr bool Scanner::match(char expected) noexcept {
    if (is_at_end()) { return false; }
    if (*current != expected) { return false; }
    current = std::next(current);
    return true;
}

[[nodiscard]] inline constexpr Token Scanner::make_token(TokenType type
) const noexcept {
    return Token{
        .type = type,
        .lexeme{start, static_cast<std::size_t>(std::distance(start, current))},
        .line = line,
        .value = Value(val_none)
    };
}

[[nodiscard]] inline constexpr Token Scanner::error_token(
    std::string_view message
) const noexcept {
    using enum TokenType;
    return Token{
        .type = ERROR,
        .lexeme = message,
        .line = line,
        .value = Value(val_none)};
}

inline constexpr void Scanner::skip_whitespace() noexcept {
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

[[nodiscard]] inline constexpr TokenType Scanner::check_keyword(
    size_t offset,
    std::string_view rest,
    TokenType type
) const noexcept {
    const auto* const sub_start = std::next(start, offset);
    if (rest == std::string_view{sub_start, current}) { return type; }
    return TokenType::IDENTIFIER;
}

[[nodiscard]] inline constexpr TokenType Scanner::identifier_type(
) const noexcept {
    switch (*start) {
        using enum TokenType;
        case 'a':
            if (std::distance(start, current) > 1) {
                switch (*std::next(start)) {
                    case 'n': return check_keyword(2, "d", AND);
                    case 's':
                        if (std::distance(start, current) > 2) {
                            switch (*std::next(start, 2)) {
                                case 'y': return check_keyword(3, "nc", ASYNC);
                            }
                        }
                        return check_keyword(2, "", AS);
                    case 'w': return check_keyword(2, "ait", AWAIT);
                }
            }
            break;
        case 'b': return check_keyword(1, "reak", BREAK);
        case 'c': return check_keyword(1, "ontinue", CONTINUE);
        case 'e': return check_keyword(1, "lse", ELSE);
        case 'f':
            if (std::distance(start, current) > 1) {
                switch (*std::next(start)) {
                    case 'a': return check_keyword(2, "lse", FALSE);
                    case 'o': return check_keyword(2, "r", FOR);
                    case 'n': return check_keyword(2, "", FN);
                }
            }
            break;
        case 'i':
            if (std::distance(start, current) > 1) {
                switch (*std::next(start)) {
                    case 'f': return check_keyword(2, "", IF);
                    case 'm': return check_keyword(2, "port", IMPORT);
                    case 'n':
                        if (std::distance(start, current) > 2) {
                            switch (*std::next(start, 2)) {
                                case 'o': return check_keyword(3, "ut", INOUT);
                            }
                        }
                        return check_keyword(2, "", IN);
                    case 's': return check_keyword(2, "", IS);
                }
            }
            break;
        case 'l':
            if (std::distance(start, current) > 1) {
                switch (*std::next(start)) {
                    case 'e': return check_keyword(2, "t", LET);
                    case 'o': return check_keyword(2, "op", LOOP);
                }
            }
            break;
        case 'n': return check_keyword(1, "il", NIL);
        case 'm': return check_keyword(1, "atch", MATCH);
        case 'o':
            if (std::distance(start, current) > 1) {
                switch (*std::next(start)) {
                    case 'u': return check_keyword(2, "t", OUT);
                    case 'r': return check_keyword(2, "", OR);
                }
            }
            break;
        case 'r': return check_keyword(1, "eturn", RETURN);
        case 's':
            if (std::distance(start, current) > 1) {
                switch (*std::next(start)) {
                    case 'e': return check_keyword(2, "lf", SELF);
                    case 't': return check_keyword(2, "ruct", STRUCT);
                    case 'u': return check_keyword(2, "per", SUPER);
                }
            }
            break;
        case 't': return check_keyword(1, "rue", TRUE);
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

[[nodiscard]] inline constexpr Token Scanner::identifier() noexcept {
    while (is_alpha(peek()) || is_digit(peek())) { advance(); }
    return make_token(identifier_type());
}

[[nodiscard]] inline constexpr Token Scanner::number() noexcept {
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
            // NOLINTNEXTLINE(*-decay)
            assert(fcr.ptr == no_space.cend());
            if (fcr.ec == std::errc::result_out_of_range) {
                return error_token("Numeric literal out of range.");
            }
            auto token = make_token(type);
            token.value = Value(value);
            return token;
        }
        case FLOAT_LITERAL: {
            float_t value = 0.0;
            [[maybe_unused]] auto fcr = std::from_chars(
                no_space.cbegin(),
                no_space.cend(),
                value
            );
            // NOLINTNEXTLINE(*-decay)
            assert(fcr.ptr == no_space.cend());
            if (fcr.ec == std::errc::result_out_of_range) {
                return error_token("Numeric literal out of range.");
            }
            auto token = make_token(type);
            token.value = Value(value);
            return token;
        }
        default: unreachable();
    }
}

[[nodiscard]] inline constexpr Token Scanner::hex_number() noexcept {
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
        // NOLINTNEXTLINE(*-magic-numbers)
        16
    );
    // NOLINTNEXTLINE(*-decay)
    assert(fcr.ptr == no_space.cend());
    if (fcr.ec == std::errc::result_out_of_range) {
        return error_token("Hexadecimal integer literal out of range.");
    }
    auto token = make_token(TokenType::INTEGER_LITERAL);
    token.value = Value(value);
    return token;
}

[[nodiscard]] inline constexpr Token Scanner::raw_string() noexcept {
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
    auto token = make_token(TokenType::STRING_LITERAL);
    const auto& lex = token.lexeme;
    const std::size_t begin = lex[3] == '\n' ? 4 : 3;
    const std::size_t end = lex[lex.length() - 4] == '\n' ? 4 : 3;
    token.value = Value(make_string(
        parent_vm,
        !parent_vm.get_options().allow_pointer_to_source_content,
        lex.substr(begin, lex.length() - begin - end)
    ));
    return token;
}

[[nodiscard]] inline constexpr std::optional<u32> Scanner::hex_escape(
    size_t digits
) noexcept {
    // NOLINTNEXTLINE(*-decay)
    assert(digits > 0);
    // NOLINTNEXTLINE(*-decay)
    assert(digits <= 8);
    const auto* const escape_start = current;
    for (int i = 0; i < digits; ++i) {
        if (!is_hex_digit(peek())) { return std::nullopt; }
        advance();
    }
    u32 value = 0;
    [[maybe_unused]] auto fcr =
        // NOLINTNEXTLINE(*-decay, *-magic-numbers)
        std::from_chars(escape_start, current, value, 16);
    // NOLINTNEXTLINE(*-decay)
    assert(fcr.ptr == current);
    // NOLINTNEXTLINE(*-decay)
    assert(fcr.ec != std::errc::result_out_of_range);
    return value;
}

[[nodiscard]] inline constexpr bool
Scanner::utf8_escape(size_t digits, DynArray<char>& dst) noexcept {
    auto value_opt = hex_escape(digits);
    if (!value_opt.has_value()) { return true; }
    auto value = static_cast<char32_t>(*value_opt);
    dst.resize(parent_vm, dst.size() + 4, '\0');
    auto* dst_it = std::prev(dst.end(), 4);
    const char32_t* src_next = nullptr;
    char* tmp_next = nullptr;
    auto result = utf8_encode(
        &value,
        std::next(&value),
        src_next,
        dst_it,
        dst.end(),
        tmp_next
    );
    if (result != std::codecvt_base::result::ok) {
        dst.resize(parent_vm, dst.size() - 4);
        return true;
    }
    dst.resize(
        parent_vm,
        static_cast<size_t>(std::distance(dst.begin(), tmp_next))
    );
    return false;
}

[[nodiscard]] inline constexpr Token Scanner::string() noexcept {
    DynArray<char> string;
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') { ++line; }
        if (peek() == '\r') { continue; }
        if (peek() == '$') {
            advance();
            if (str_interp_braces.size() < MAX_INTERP_DEPTH) {
                if (peek() != '{') {
                    string.destroy(parent_vm);
                    return error_token("Expect '{' after '$'.");
                }
                str_interp_braces.push_back(1);
                advance();
                auto token = make_token(TokenType::STRING_INTERP);
                token.value = Value(make_string(
                    parent_vm,
                    true,
                    std::string_view{
                        std::next(string.begin(), 1),
                        std::prev(string.end(), 2)}
                ));
                string.destroy(parent_vm);
                return token;
            }
            string.destroy(parent_vm);
            return error_token("Nested string interpolation too deep.");
        }
        if (peek() == '\\') {
            advance();
            switch (peek()) {
                case '\\':
                    advance();
                    string.push_back(parent_vm, '\\');
                    break;
                case '"':
                    advance();
                    string.push_back(parent_vm, '"');
                    break;
                case '$':
                    advance();
                    string.push_back(parent_vm, '$');
                    break;
                case '0':
                    advance();
                    string.push_back(parent_vm, '\0');
                    break;
                case 'a':
                    advance();
                    string.push_back(parent_vm, '\a');
                    break;
                case 'b':
                    advance();
                    string.push_back(parent_vm, '\b');
                    break;
                case 'e':
                    advance();
                    string.push_back(parent_vm, '\33');
                    break;
                case 'f':
                    advance();
                    string.push_back(parent_vm, '\f');
                    break;
                case 'n':
                    advance();
                    string.push_back(parent_vm, '\n');
                    break;
                case 'r':
                    advance();
                    string.push_back(parent_vm, '\r');
                    break;
                case 't':
                    advance();
                    string.push_back(parent_vm, '\t');
                    break;
                case 'v':
                    advance();
                    string.push_back(parent_vm, '\v');
                    break;
                case 'x': {
                    advance();
                    auto value_opt = hex_escape(2);
                    if (!value_opt.has_value()) {
                        string.destroy(parent_vm);
                        return error_token("Invalid byte escape sequence.");
                    }
                    string.push_back(parent_vm, static_cast<char>(*value_opt));
                    break;
                }
                case 'u': {
                    advance();
                    if (utf8_escape(4, string)) {
                        string.destroy(parent_vm);
                        return error_token(
                            "Invalid 16-bits Unicode escape sequence."
                        );
                    }
                    break;
                }
                case 'U': {
                    advance();
                    // NOLINTNEXTLINE(*-magic-numbers)
                    if (utf8_escape(8, string)) {
                        string.destroy(parent_vm);
                        return error_token(
                            "Invalid 32-bits Unicode escape sequence."
                        );
                    }
                    break;
                }
                default:
                    string.destroy(parent_vm);
                    return error_token("Invalid escape character.");
            }
        } else {
            string.push_back(parent_vm, advance());
        }
    }
    if (is_at_end()) {
        string.destroy(parent_vm);
        return error_token("Unterminated string.");
    }
    advance();
    auto token = make_token(TokenType::STRING_LITERAL);
    token.value = Value(make_string(
        parent_vm,
        true,
        std::string_view{string.begin(), string.end()}
    ));
    string.destroy(parent_vm);
    return token;
}

[[nodiscard]] inline constexpr Token Scanner::scan_token() noexcept {
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
