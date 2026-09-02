#include "token.hpp"

namespace sutro {

const char* tokenKindName(TokenKind kind) {
    switch (kind) {
        case TokenKind::EndOfFile:    return "EOF";
        case TokenKind::Identifier:   return "IDENT";
        case TokenKind::IntLiteral:   return "INT";
        case TokenKind::FloatLiteral: return "FLOAT";
        case TokenKind::KwPurno:      return "TYPE";
        case TokenKind::KwDoshomik:   return "TYPE";
        case TokenKind::KwJodi:       return "IF";
        case TokenKind::KwNahole:     return "ELSE";
        case TokenKind::KwJotokkhon:  return "WHILE";
        case TokenKind::KwDekhao:     return "PRINT";
        case TokenKind::Dari:         return "DARI";
        case TokenKind::Assign:       return "ASSIGN";
        case TokenKind::Plus:         return "PLUS";
        case TokenKind::Minus:        return "MINUS";
        case TokenKind::Star:         return "STAR";
        case TokenKind::Slash:        return "SLASH";
        case TokenKind::LParen:       return "LPAREN";
        case TokenKind::RParen:       return "RPAREN";
        case TokenKind::LBrace:       return "LBRACE";
        case TokenKind::RBrace:       return "RBRACE";
        case TokenKind::Less:         return "LT";
        case TokenKind::Greater:      return "GT";
        case TokenKind::LessEqual:    return "LE";
        case TokenKind::GreaterEqual: return "GE";
        case TokenKind::EqualEqual:   return "EQ";
        case TokenKind::BangEqual:    return "NE";
        case TokenKind::Error:        return "ERROR";
    }
    return "?";
}

std::string describe(TokenKind kind) {
    switch (kind) {
        case TokenKind::EndOfFile:    return "ফাইলের শেষ";
        case TokenKind::Identifier:   return "একটি নাম";
        case TokenKind::IntLiteral:   return "একটি পূর্ণসংখ্যা";
        case TokenKind::FloatLiteral: return "একটি দশমিক সংখ্যা";
        case TokenKind::KwPurno:      return "পূর্ণ";
        case TokenKind::KwDoshomik:   return "দশমিক";
        case TokenKind::KwJodi:       return "যদি";
        case TokenKind::KwNahole:     return "নাহলে";
        case TokenKind::KwJotokkhon:  return "যতক্ষণ";
        case TokenKind::KwDekhao:     return "দেখাও";
        case TokenKind::Dari:         return "দাঁড়ি (।)";
        case TokenKind::Assign:       return "'='";
        case TokenKind::Plus:         return "'+'";
        case TokenKind::Minus:        return "'-'";
        case TokenKind::Star:         return "'*'";
        case TokenKind::Slash:        return "'/'";
        case TokenKind::LParen:       return "'('";
        case TokenKind::RParen:       return "')'";
        case TokenKind::LBrace:       return "'{'";
        case TokenKind::RBrace:       return "'}'";
        case TokenKind::Less:         return "'<'";
        case TokenKind::Greater:      return "'>'";
        case TokenKind::LessEqual:    return "'<='";
        case TokenKind::GreaterEqual: return "'>='";
        case TokenKind::EqualEqual:   return "'=='";
        case TokenKind::BangEqual:    return "'!='";
        case TokenKind::Error:        return "অবৈধ অক্ষর";
    }
    return "?";
}

bool isTypeKeyword(TokenKind kind) {
    return kind == TokenKind::KwPurno || kind == TokenKind::KwDoshomik;
}

} // namespace sutro
