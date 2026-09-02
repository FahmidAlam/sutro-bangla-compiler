// The lexer's output alphabet.
//
// A token carries its own position because every later phase — the parser, the
// type checker — reports errors against the token it was looking at, and the
// diagnostics engine needs a line and column to draw the caret.
#pragma once

#include <string>

namespace sutro {

enum class TokenKind {
    EndOfFile,

    Identifier,
    IntLiteral,
    FloatLiteral,

    KwPurno,        // পূর্ণ      integer type
    KwDoshomik,     // দশমিক     floating type
    KwJodi,         // যদি        if
    KwNahole,       // নাহলে      else
    KwJotokkhon,    // যতক্ষণ     while
    KwDekhao,       // দেখাও      print

    Dari,           // ।   statement terminator
    Assign,         // =
    Plus, Minus, Star, Slash,
    LParen, RParen, LBrace, RBrace,
    Less, Greater, LessEqual, GreaterEqual, EqualEqual, BangEqual,

    Error           // produced only so the stream stays well-formed after a bad char
};

struct Token {
    TokenKind kind = TokenKind::EndOfFile;
    std::string lexeme;         // exactly as written in the source
    long long intValue = 0;     // meaningful when kind == IntLiteral
    double floatValue = 0.0;    // meaningful when kind == FloatLiteral
    int line = 0;               // 1-based
    int column = 0;             // 1-based, in visible characters
    int length = 0;             // visible characters, for the caret
};

// Short name used by --emit=tokens, e.g. TYPE, IDENT, INT, DARI.
const char* tokenKindName(TokenKind kind);

// The name a student would recognise, used inside Bangla error messages.
std::string describe(TokenKind kind);

bool isTypeKeyword(TokenKind kind);

} // namespace sutro
