// Hand-written scanner: UTF-8 bytes in, tokens out.
//
// It never stops at the first bad character. An unknown character is reported and
// skipped, so one typo does not hide the rest of the file's tokens.
#pragma once

#include <string>
#include <vector>

#include "diagnostics.hpp"
#include "source.hpp"
#include "token.hpp"
#include "utf8.hpp"

namespace sutro {

class Lexer {
public:
    Lexer(const SourceFile& source, DiagnosticBag& diagnostics);

    // Always returns a stream ending in one EndOfFile token.
    std::vector<Token> tokenize();

private:
    char32_t peek(int ahead = 0) const;   // codepoint `ahead` characters from here
    char32_t advance();                   // consume one codepoint, track line/column
    bool match(char32_t expected);        // consume it only if it is what we expect
    bool atEnd() const { return pos_ >= text_.size(); }

    Token make(TokenKind kind, const std::string& lexeme, int line, int column) const;

    void skipTrivia();                    // spaces, newlines, // and /* */ comments
    Token scanNumber();                   // ০-৯ and 0-9, with an optional decimal part
    Token scanIdentifierOrKeyword();

    const SourceFile& source_;
    const std::string& text_;
    DiagnosticBag& diags_;

    std::size_t pos_ = 0;    // byte offset into text_
    int line_ = 1;
    int column_ = 1;         // visible characters, so matras do not advance it
};

} // namespace sutro
