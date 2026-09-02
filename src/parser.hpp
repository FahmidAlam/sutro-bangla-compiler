// Recursive descent, written by hand — one function per grammar rule, so the code
// and the BNF in the report can be read side by side.
//
// Precedence is structural: condition -> expression -> term -> unary -> primary.
// Because '*' is parsed one level below '+', it ends up deeper in the tree, and
// nothing anywhere has to consult a precedence table.
//
// Error handling is panic mode. A failed rule throws an internal ParseError to
// unwind out of however deep the expression recursion went; statement() catches
// it, and synchronize() then discards tokens up to the next দাঁড়ি so the parser
// can start the following statement cleanly. The throw never escapes parse().
#pragma once

#include <initializer_list>
#include <string>
#include <vector>

#include "ast.hpp"
#include "diagnostics.hpp"
#include "token.hpp"

namespace sutro {

class Parser {
public:
    Parser(const std::vector<Token>& tokens, DiagnosticBag& diagnostics);

    // Always returns a Program. On errors it holds the statements that did parse,
    // and the diagnostics bag holds one entry per broken statement.
    Program parse();

private:
    struct ParseError {};   // internal control flow only

    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    bool check(TokenKind kind) const;
    const Token& advance();
    bool match(std::initializer_list<TokenKind> kinds);

    // Consumes the expected token, or reports and throws.
    const Token& expect(TokenKind kind, const std::string& code,
                        const std::string& message, const std::string& hint);
    [[noreturn]] void fail(const Token& at, const std::string& code,
                           const std::string& message, const std::string& hint);

    // Reports a missing দাঁড়ি at the right spot but does not throw — see the
    // comment on the definition for why this one error needs no panic.
    void expectDari();

    void synchronize();

    StmtPtr statement();
    StmtPtr declaration();
    StmtPtr assignment();
    StmtPtr printStatement();
    StmtPtr ifStatement();
    StmtPtr whileStatement();
    std::vector<StmtPtr> block();

    ExprPtr condition();     // one comparison: expr <relop> expr
    ExprPtr expression();    // + and -
    ExprPtr term();          // * and /
    ExprPtr unary();         // prefix -
    ExprPtr primary();       // literal, name, or ( expr )

    const std::vector<Token>& tokens_;
    DiagnosticBag& diags_;
    std::size_t current_ = 0;
};

} // namespace sutro
