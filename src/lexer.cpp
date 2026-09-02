#include "lexer.hpp"

#include <cerrno>
#include <cstdlib>
#include <unordered_map>

namespace sutro {

Lexer::Lexer(const SourceFile& source, DiagnosticBag& diagnostics)
    : source_(source), text_(source.text()), diags_(diagnostics) {}

char32_t Lexer::peek(int ahead) const {
    std::size_t i = pos_;
    for (int k = 0; k < ahead && i < text_.size(); ++k) {
        i += static_cast<std::size_t>(utf8::decode(text_, i).bytes);
    }
    if (i >= text_.size()) return 0;
    return utf8::decode(text_, i).cp;
}

char32_t Lexer::advance() {
    const auto d = utf8::decode(text_, pos_);
    pos_ += static_cast<std::size_t>(d.bytes);
    if (d.cp == '\n') {
        ++line_;
        column_ = 1;
    } else if (!utf8::isCombiningMark(d.cp)) {
        ++column_;   // a matra rides on the previous letter and takes no column
    }
    return d.cp;
}

bool Lexer::match(char32_t expected) {
    if (atEnd() || peek() != expected) return false;
    advance();
    return true;
}

Token Lexer::make(TokenKind kind, const std::string& lexeme, int line, int column) const {
    Token t;
    t.kind = kind;
    t.lexeme = lexeme;
    t.line = line;
    t.column = column;
    // We are called after the token has been consumed, so the column we are on now
    // minus the column we started at is exactly its visible width.
    t.length = (line == line_ && column_ > column) ? column_ - column : 1;
    return t;
}

void Lexer::skipTrivia() {
    while (!atEnd()) {
        const char32_t c = peek();

        if (utf8::isSpace(c)) {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/') {              // line comment
            while (!atEnd() && peek() != '\n') advance();
            continue;
        }
        if (c == '/' && peek(1) == '*') {              // block comment
            const int startLine = line_, startCol = column_;
            advance();
            advance();
            bool closed = false;
            while (!atEnd()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                diags_.error("L04", "মন্তব্য শেষ হয়নি",
                             "'*/' দিয়ে মন্তব্যটি বন্ধ করুন", startLine, startCol, 2);
            }
            continue;
        }
        break;
    }
}

Token Lexer::scanNumber() {
    const int line = line_, col = column_;

    std::string lexeme;   // as written: may be ১০
    std::string ascii;    // normalised: always 10
    bool isFloat = false;

    auto takeDigits = [&]() {
        while (!atEnd() && utf8::digitValue(peek()) >= 0) {
            const int v = utf8::digitValue(peek());
            lexeme += utf8::encode(advance());
            ascii += static_cast<char>('0' + v);
        }
    };

    takeDigits();

    // A '.' only starts a decimal part if a digit follows it, so "১।" still lexes
    // as an integer followed by the terminator.
    if (!atEnd() && peek() == '.' && utf8::digitValue(peek(1)) >= 0) {
        isFloat = true;
        advance();
        lexeme += '.';
        ascii += '.';
        takeDigits();
    }

    if (!atEnd() && utf8::isIdentStart(peek())) {
        diags_.error("L03", "সংখ্যার ঠিক পরে নাম লেখা যাবে না",
                     "সংখ্যা ও নামের মাঝে একটি ফাঁকা জায়গা দিন", line_, column_, 1);
    }

    Token t = make(isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral, lexeme, line, col);
    if (isFloat) {
        t.floatValue = std::strtod(ascii.c_str(), nullptr);
    } else {
        errno = 0;
        t.intValue = std::strtoll(ascii.c_str(), nullptr, 10);
        if (errno == ERANGE) {
            diags_.error("L05", "সংখ্যাটি অনেক বড়",
                         "পূর্ণ সংখ্যার সর্বোচ্চ মান পেরিয়ে গেছে", line, col, t.length);
            t.intValue = 0;
        }
    }
    return t;
}

Token Lexer::scanIdentifierOrKeyword() {
    const int line = line_, col = column_;

    std::string lexeme;
    while (!atEnd() && utf8::isIdentContinue(peek())) {
        lexeme += utf8::encode(advance());
    }

    // The keyword is matched by its exact UTF-8 bytes. That is fine because every
    // Bangla keyboard produces these letters in the same canonical order.
    static const std::unordered_map<std::string, TokenKind> keywords = {
        {"পূর্ণ",   TokenKind::KwPurno},
        {"দশমিক",  TokenKind::KwDoshomik},
        {"যদি",     TokenKind::KwJodi},
        {"নাহলে",   TokenKind::KwNahole},
        {"যতক্ষণ",  TokenKind::KwJotokkhon},
        {"দেখাও",   TokenKind::KwDekhao},
    };

    const auto it = keywords.find(lexeme);
    return make(it != keywords.end() ? it->second : TokenKind::Identifier, lexeme, line, col);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    for (;;) {
        skipTrivia();
        if (atEnd()) {
            tokens.push_back(make(TokenKind::EndOfFile, "", line_, column_));
            break;
        }

        const char32_t c = peek();

        if (utf8::digitValue(c) >= 0) {
            tokens.push_back(scanNumber());
            continue;
        }
        if (utf8::isIdentStart(c)) {
            tokens.push_back(scanIdentifierOrKeyword());
            continue;
        }

        const int line = line_, col = column_;
        advance();                       // consume the punctuation character

        switch (c) {
            case utf8::DARI: tokens.push_back(make(TokenKind::Dari,   "।", line, col)); break;
            case '+':        tokens.push_back(make(TokenKind::Plus,   "+", line, col)); break;
            case '-':        tokens.push_back(make(TokenKind::Minus,  "-", line, col)); break;
            case '*':        tokens.push_back(make(TokenKind::Star,   "*", line, col)); break;
            case '/':        tokens.push_back(make(TokenKind::Slash,  "/", line, col)); break;
            case '(':        tokens.push_back(make(TokenKind::LParen, "(", line, col)); break;
            case ')':        tokens.push_back(make(TokenKind::RParen, ")", line, col)); break;
            case '{':        tokens.push_back(make(TokenKind::LBrace, "{", line, col)); break;
            case '}':        tokens.push_back(make(TokenKind::RBrace, "}", line, col)); break;

            case '=':
                if (match('=')) tokens.push_back(make(TokenKind::EqualEqual, "==", line, col));
                else            tokens.push_back(make(TokenKind::Assign,     "=",  line, col));
                break;
            case '<':
                if (match('=')) tokens.push_back(make(TokenKind::LessEqual,  "<=", line, col));
                else            tokens.push_back(make(TokenKind::Less,       "<",  line, col));
                break;
            case '>':
                if (match('=')) tokens.push_back(make(TokenKind::GreaterEqual, ">=", line, col));
                else            tokens.push_back(make(TokenKind::Greater,      ">",  line, col));
                break;
            case '!':
                if (match('=')) {
                    tokens.push_back(make(TokenKind::BangEqual, "!=", line, col));
                } else {
                    diags_.error("L02", "'!' একা ব্যবহার করা যায় না",
                                 "সমান নয় বোঝাতে '!=' লিখুন", line, col, 1);
                }
                break;

            case ';':   // the mistake a student who knows Java will make first
                diags_.error("L06", "এখানে সেমিকোলন চলবে না",
                             "প্রতিটি লাইন দাঁড়ি (।) দিয়ে শেষ করুন", line, col, 1);
                break;

            default:
                diags_.error("L01", "অপ্রত্যাশিত অক্ষর '" + utf8::encode(c) + "'",
                             "এই অক্ষরটি সূত্র ভাষার অংশ নয়", line, col, 1);
                break;
        }
    }

    return tokens;
}

} // namespace sutro
