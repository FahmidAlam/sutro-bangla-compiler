// UTF-8 decoding and Bangla character classification.
//
// The whole compiler works on codepoints, never on raw bytes, because a Bangla
// letter is three bytes in UTF-8. Everything that needs to ask "is this a digit,
// a letter, a space?" comes through here.
#pragma once

#include <cstdint>
#include <string>

namespace sutro {
namespace utf8 {

// The Bangla full stop, our statement terminator. It lives in the Devanagari
// block, NOT the Bengali block, which is why it can never collide with a letter.
constexpr char32_t DARI = 0x0964;

struct Decoded {
    char32_t cp;    // the decoded codepoint
    int bytes;      // how many bytes it occupied
};

// Decodes the codepoint starting at byte offset `i`. A malformed byte decodes to
// U+FFFD with bytes == 1, so the lexer can always move forward and never hangs.
Decoded decode(const std::string& s, std::size_t i);

// Turns a codepoint back into UTF-8 bytes (used to quote a character in an error).
std::string encode(char32_t cp);

bool isAsciiDigit(char32_t cp);
bool isBanglaDigit(char32_t cp);      // ০..৯   U+09E6..U+09EF
int  digitValue(char32_t cp);         // 0..9, or -1 if it is not a digit at all

bool isIdentStart(char32_t cp);       // ASCII letter, '_', or any Bengali letter
bool isIdentContinue(char32_t cp);    // the above, plus digits of either script

// Matras, hasanta and other signs hang off the previous letter instead of taking
// their own space, so we skip them when counting columns for the error caret.
bool isCombiningMark(char32_t cp);

bool isSpace(char32_t cp);

// 12 -> "১২". Line and column numbers are shown in Bangla in Bangla messages.
std::string toBanglaDigits(long long n);

} // namespace utf8
} // namespace sutro
