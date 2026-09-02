// Semantic analysis: the phase between "this parses" and "this means something".
//
// It walks the tree the parser produced and does three things:
//
//   1. builds the symbol table — every declaration, with its type and scope;
//   2. decides a Type for every expression node, so the IR builder never has to
//      re-derive one;
//   3. inserts Widen nodes wherever an int has to become a float, so the widening
//      is written down in the tree instead of being rediscovered by each backend.
//
// It reports into the same DiagnosticBag as the lexer and parser and never
// throws: an expression it cannot type becomes Type::Error, which every rule
// treats as "already complained about, stay quiet", so one mistake produces one
// message rather than a cascade.
#pragma once

#include "ast.hpp"
#include "diagnostics.hpp"
#include "symbols.hpp"
#include "token.hpp"
#include "type.hpp"

namespace sutro {

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(DiagnosticBag& diagnostics);

    // Mutates the tree: fills in Expr::type and inserts Widen nodes.
    void analyze(Program& program);

    const SymbolTable& symbols() const { return symbols_; }

private:
    void statement(Stmt* s);
    void block(std::vector<StmtPtr>& body);

    void declareStmt(Stmt& s);
    void assignStmt(Stmt& s);
    void conditionOf(Stmt& s);

    Type expression(ExprPtr& e);
    Type binary(Expr& e);

    // target <- value. Widens an int value for a float target; refuses the other
    // direction, since a silently dropped fraction is the exact bug a type
    // checker exists to catch.
    void checkAssignable(Type target, ExprPtr& value, const Token& at,
                         const std::string& name);

    // Wraps `e` in a Widen node in place. Only ever called when e is typed Int.
    static void widen(ExprPtr& e);

    void warnUnused();

    // This phase walks the tree by recursion, so an expression's depth is also
    // its stack depth. Student code is a handful of levels deep; a few thousand
    // — which a generated file or a stress test can reach — would overflow the
    // stack and kill the process, and §2.1 forbids crashing on *any* input.
    // Refusing the expression with a diagnostic keeps that promise.
    static constexpr int kMaxExprDepth = 256;

    DiagnosticBag& diags_;
    SymbolTable symbols_;
    int depth_ = 0;
    bool depthReported_ = false;   // one complaint per program, not per level
};

} // namespace sutro
