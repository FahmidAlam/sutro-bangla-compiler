#include "parser.hpp"

namespace sutro {

Parser::Parser(const std::vector<Token>& tokens, DiagnosticBag& diagnostics)
    : tokens_(tokens), diags_(diagnostics) {}

// ---------------------------------------------------------------- token access

const Token& Parser::peek() const { return tokens_[current_]; }

const Token& Parser::previous() const {
    return tokens_[current_ == 0 ? 0 : current_ - 1];
}

bool Parser::isAtEnd() const { return peek().kind == TokenKind::EndOfFile; }

bool Parser::check(TokenKind kind) const { return peek().kind == kind; }

const Token& Parser::advance() {
    if (!isAtEnd()) ++current_;
    return previous();
}

bool Parser::match(std::initializer_list<TokenKind> kinds) {
    for (TokenKind k : kinds) {
        if (check(k)) {
            advance();
            return true;
        }
    }
    return false;
}

const Token& Parser::expect(TokenKind kind, const std::string& code,
                            const std::string& message, const std::string& hint) {
    if (check(kind)) return advance();
    fail(peek(), code, message, hint);
}

void Parser::fail(const Token& at, const std::string& code,
                  const std::string& message, const std::string& hint) {
    diags_.error(code, message, hint, at.line, at.column,
                 at.length > 0 ? at.length : 1);
    throw ParseError{};
}

// The missing দাঁড়ি is the one error we can recover from without throwing any
// tokens away: the statement has already been parsed, so we know exactly where
// the terminator belonged. We report it at the end of the last token we read —
// which is where the student has to type it — and then carry on as if it had
// been written. Every other failure has to panic and skip.
void Parser::expectDari() {
    if (check(TokenKind::Dari)) {
        advance();
        return;
    }
    const Token& prev = previous();
    diags_.error("P01", "বিবৃতির শেষে দাঁড়ি (।) দরকার",
                 "প্রতিটি বিবৃতি দাঁড়ি দিয়ে শেষ হয়",
                 prev.line, prev.column + prev.length, 1);
}

// ------------------------------------------------------------------- recovery

// Panic mode. Throw away tokens until we are confident we stand at the start of
// a fresh statement. The দাঁড়ি does the work: it is required at the end of every
// simple statement and appears nowhere else, so seeing one means "the broken
// statement ended here". '}' and EOF are the secondary anchors for when the
// missing token *was* the দাঁড়ি.
void Parser::synchronize() {
    // Always drop at least one token, otherwise a token that no rule accepts
    // would be re-parsed forever.
    if (!isAtEnd()) advance();

    while (!isAtEnd()) {
        if (previous().kind == TokenKind::Dari) return;   // just passed a terminator

        switch (peek().kind) {
            case TokenKind::KwPurno:
            case TokenKind::KwDoshomik:
            case TokenKind::KwJodi:
            case TokenKind::KwJotokkhon:
            case TokenKind::KwDekhao:
            case TokenKind::LBrace:
            case TokenKind::RBrace:
                return;                                   // a statement can start here
            default:
                advance();
        }
    }
}

// ------------------------------------------------------------------ statements

Program Parser::parse() {
    Program program;

    while (!isAtEnd()) {
        try {
            StmtPtr s = statement();
            if (s) program.statements.push_back(std::move(s));
        } catch (const ParseError&) {
            synchronize();
        }
    }
    return program;
}

StmtPtr Parser::statement() {
    if (isTypeKeyword(peek().kind))          return declaration();
    if (check(TokenKind::KwDekhao))          return printStatement();
    if (check(TokenKind::KwJodi))            return ifStatement();
    if (check(TokenKind::KwJotokkhon))       return whileStatement();
    if (check(TokenKind::Identifier))        return assignment();

    if (check(TokenKind::LBrace)) {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Block;
        s->token = peek();
        s->body = block();
        return s;
    }

    // A lone দাঁড়ি is harmless — an empty statement. Swallow it quietly rather
    // than reporting an error the student cannot act on.
    if (match({TokenKind::Dari})) return nullptr;

    fail(peek(), "P08", "এখানে একটি বিবৃতি আশা করা হয়েছিল, পাওয়া গেছে " + describe(peek().kind),
         "লাইনটি পূর্ণ, দশমিক, দেখাও, যদি, যতক্ষণ বা একটি নাম দিয়ে শুরু করুন");
}

// পূর্ণ ক = ১০।
StmtPtr Parser::declaration() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Declare;
    s->token = advance();                    // the type keyword
    s->declaredType = s->token.kind;

    const Token& name = expect(TokenKind::Identifier, "P02",
        "টাইপের পরে একটি নাম দরকার",
        "যেমন: " + s->token.lexeme + " ক = ১০।");
    s->name = name.lexeme;

    expect(TokenKind::Assign, "P03", "নাম ও মানের মাঝে '=' দরকার",
           "ঘোষণার সময় একটি প্রাথমিক মান দিতে হয়");
    s->value = expression();

    expectDari();
    return s;
}

// ক = ক + ১।
StmtPtr Parser::assignment() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Assign;
    s->token = advance();                    // the name
    s->name = s->token.lexeme;

    expect(TokenKind::Assign, "P03", "নামের পরে '=' দরকার",
           "মান বসাতে হলে লিখুন: " + s->name + " = ...।");
    s->value = expression();

    expectDari();
    return s;
}

// দেখাও(ফল)।
StmtPtr Parser::printStatement() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Print;
    s->token = advance();                    // দেখাও

    expect(TokenKind::LParen, "P04", "দেখাও-এর পরে '(' দরকার", "লিখুন: দেখাও(...)।");
    s->value = expression();
    expect(TokenKind::RParen, "P05", "')' বন্ধ করা হয়নি", "প্রতিটি '(' এর জন্য একটি ')' লাগে");

    expectDari();
    return s;
}

// যদি (ক > ০) { ... } নাহলে { ... }
StmtPtr Parser::ifStatement() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::If;
    s->token = advance();                    // যদি

    expect(TokenKind::LParen, "P04", "যদি-এর পরে '(' দরকার", "লিখুন: যদি (ক > ০) { ... }");
    s->value = condition();
    expect(TokenKind::RParen, "P05", "')' বন্ধ করা হয়নি", "শর্তের পরে ')' দিন");

    s->body = block();

    if (match({TokenKind::KwNahole})) {
        // নাহলে যদি (...) chains as a nested if inside the else branch.
        if (check(TokenKind::KwJodi)) {
            s->elseBody.push_back(ifStatement());
        } else {
            s->elseBody = block();
        }
    }
    return s;
}

// যতক্ষণ (ন > ০) { ... }
StmtPtr Parser::whileStatement() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::While;
    s->token = advance();                    // যতক্ষণ

    expect(TokenKind::LParen, "P04", "যতক্ষণ-এর পরে '(' দরকার",
           "লিখুন: যতক্ষণ (ন > ০) { ... }");
    s->value = condition();
    expect(TokenKind::RParen, "P05", "')' বন্ধ করা হয়নি", "শর্তের পরে ')' দিন");

    s->body = block();
    return s;
}

std::vector<StmtPtr> Parser::block() {
    expect(TokenKind::LBrace, "P06", "'{' দিয়ে ব্লক শুরু করতে হবে",
           "শর্ত বা লুপের পরে { } এর ভেতরে বিবৃতি লিখুন");

    std::vector<StmtPtr> body;
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        try {
            StmtPtr s = statement();
            if (s) body.push_back(std::move(s));
        } catch (const ParseError&) {
            synchronize();
            // synchronize() stops on '}' too, so a broken statement inside a
            // block does not eat the rest of the block.
        }
    }

    expect(TokenKind::RBrace, "P07", "'}' দিয়ে ব্লক শেষ করা হয়নি",
           "প্রতিটি '{' এর জন্য একটি '}' লাগে");
    return body;
}

// ----------------------------------------------------------------- expressions

// A condition is exactly one comparison. সূত্র has no boolean type, so a
// comparison is only meaningful here — which also means `যদি (ক)` is caught as
// an error instead of silently meaning something.
ExprPtr Parser::condition() {
    ExprPtr left = expression();

    if (!match({TokenKind::Less, TokenKind::Greater, TokenKind::LessEqual,
                TokenKind::GreaterEqual, TokenKind::EqualEqual, TokenKind::BangEqual})) {
        fail(peek(), "P10", "শর্তে একটি তুলনা দরকার",
             "যেমন: ন > ০ অথবা ক == ৫");
    }

    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::Binary;
    e->token = previous();
    e->op = previous().kind;
    e->lhs = std::move(left);
    e->rhs = expression();
    return e;
}

ExprPtr Parser::expression() {
    ExprPtr left = term();

    while (match({TokenKind::Plus, TokenKind::Minus})) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Binary;
        e->token = previous();
        e->op = previous().kind;
        e->lhs = std::move(left);
        e->rhs = term();
        left = std::move(e);
    }
    return left;
}

ExprPtr Parser::term() {
    ExprPtr left = unary();

    while (match({TokenKind::Star, TokenKind::Slash})) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Binary;
        e->token = previous();
        e->op = previous().kind;
        e->lhs = std::move(left);
        e->rhs = unary();
        left = std::move(e);
    }
    return left;
}

ExprPtr Parser::unary() {
    if (match({TokenKind::Minus})) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Unary;
        e->token = previous();
        e->op = previous().kind;
        e->lhs = unary();
        return e;
    }
    return primary();
}

ExprPtr Parser::primary() {
    if (check(TokenKind::IntLiteral)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::IntLit;
        e->token = advance();
        e->intValue = e->token.intValue;
        return e;
    }
    if (check(TokenKind::FloatLiteral)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::FloatLit;
        e->token = advance();
        e->floatValue = e->token.floatValue;
        return e;
    }
    if (check(TokenKind::Identifier)) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Name;
        e->token = advance();
        e->name = e->token.lexeme;
        return e;
    }
    if (match({TokenKind::LParen})) {
        ExprPtr inner = expression();
        expect(TokenKind::RParen, "P05", "')' বন্ধ করা হয়নি",
               "প্রতিটি '(' এর জন্য একটি ')' লাগে");
        return inner;
    }

    fail(peek(), "P09", "এখানে একটি মান আশা করা হয়েছিল, পাওয়া গেছে " + describe(peek().kind),
         "একটি সংখ্যা, একটি নাম, অথবা বন্ধনীতে একটি হিসাব লিখুন");
}

} // namespace sutro
