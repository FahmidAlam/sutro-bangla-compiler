#include "utf8.hpp"

namespace sutro {
namespace utf8 {

Decoded decode(const std::string& s, std::size_t i) {
    if (i >= s.size()) return {0, 0};

    const unsigned char b0 = static_cast<unsigned char>(s[i]);
    const std::size_t left = s.size() - i;

    auto cont = [&](std::size_t k) -> bool {          // is s[i+k] a 10xxxxxx byte?
        return k < left && (static_cast<unsigned char>(s[i + k]) & 0xC0) == 0x80;
    };
    auto tail = [&](std::size_t k) -> char32_t {
        return static_cast<char32_t>(static_cast<unsigned char>(s[i + k]) & 0x3F);
    };

    if (b0 < 0x80) return {b0, 1};                                   // 0xxxxxxx
    if ((b0 & 0xE0) == 0xC0 && cont(1))                              // 110xxxxx
        return {static_cast<char32_t>((b0 & 0x1F) << 6 | tail(1)), 2};
    if ((b0 & 0xF0) == 0xE0 && cont(1) && cont(2))                   // 1110xxxx
        return {static_cast<char32_t>((b0 & 0x0F) << 12 | tail(1) << 6 | tail(2)), 3};
    if ((b0 & 0xF8) == 0xF0 && cont(1) && cont(2) && cont(3))        // 11110xxx
        return {static_cast<char32_t>((b0 & 0x07) << 18 | tail(1) << 12 | tail(2) << 6 | tail(3)), 4};

    return {0xFFFD, 1};   // malformed: report it, but keep moving
}

std::string encode(char32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

bool isAsciiDigit(char32_t cp)  { return cp >= '0' && cp <= '9'; }
bool isBanglaDigit(char32_t cp) { return cp >= 0x09E6 && cp <= 0x09EF; }

int digitValue(char32_t cp) {
    if (isAsciiDigit(cp))  return static_cast<int>(cp - '0');
    if (isBanglaDigit(cp)) return static_cast<int>(cp - 0x09E6);
    return -1;
}

// The Bengali block is U+0980..U+09FF. Everything in it is part of a word except
// the ten digits, so "is it a letter" is a range test minus one hole.
static bool isBengaliLetterish(char32_t cp) {
    return cp >= 0x0980 && cp <= 0x09FF && !isBanglaDigit(cp);
}

bool isIdentStart(char32_t cp) {
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_'
        || isBengaliLetterish(cp);
}

bool isIdentContinue(char32_t cp) {
    return isIdentStart(cp) || isAsciiDigit(cp) || isBanglaDigit(cp);
}

bool isCombiningMark(char32_t cp) {
    return (cp >= 0x0981 && cp <= 0x0983)     // চন্দ্রবিন্দু, অনুস্বার, বিসর্গ
        || (cp >= 0x09BC && cp <= 0x09CD)     // নুক্তা, matras, হসন্ত
        ||  cp == 0x09D7                      // ৗ
        || (cp >= 0x09E2 && cp <= 0x09E3);
}

bool isSpace(char32_t cp) {
    return cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n' || cp == 0x00A0;
}

std::string toBanglaDigits(long long n) {
    if (n == 0) return "০";              // ০
    std::string sign = n < 0 ? "-" : "";
    unsigned long long v = n < 0 ? static_cast<unsigned long long>(-n)
                                 : static_cast<unsigned long long>(n);
    std::string rev;
    while (v > 0) {
        rev += encode(static_cast<char32_t>(0x09E6 + (v % 10)));
        v /= 10;
    }
    // rev holds whole multi-byte characters, so reverse per character, not per byte.
    std::string out = sign;
    for (std::size_t i = rev.size(); i > 0; i -= 3) out += rev.substr(i - 3, 3);
    return out;
}

// Column width in visible characters. A Bangla letter is three bytes and a matra
// takes no column of its own, so neither .size() nor a codepoint count would line
// a table up correctly.
std::size_t visibleWidth(const std::string& s) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < s.size();) {
        const Decoded d = decode(s, i);
        if (!isCombiningMark(d.cp)) ++n;
        i += static_cast<std::size_t>(d.bytes);
    }
    return n;
}

std::string padTo(const std::string& s, std::size_t width) {
    const std::size_t w = visibleWidth(s);
    return w >= width ? s + " " : s + std::string(width - w, ' ');
}

} // namespace utf8
} // namespace sutro
