// The syntax tree the parser hands to the semantic analyser.
//
// Both Expr and Stmt are single structs with a `kind` tag rather than a class
// hierarchy. That means no virtual calls and no casts anywhere in the compiler:
// every phase is a switch on `kind`, which is the shape a reader can follow
// top-to-bottom. The cost is a few unused fields per node, which at this size is
// cheaper than the indirection would be to explain.
//
// Ownership is unique_ptr all the way down, so the whole tree dies with Program.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "token.hpp"
#include "type.hpp"

namespace sutro {

struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

enum class ExprKind {
    IntLit,     // ১০
    FloatLit,   // ১.৫
    Name,       // ক
    Unary,      // -ক
    Binary      // ক + ২,  ক > ০
};

struct Expr {
    ExprKind kind;
    Token token;                  // where it came from — every error needs a position

    long long intValue = 0;       // IntLit
    double floatValue = 0.0;      // FloatLit
    std::string name;             // Name

    TokenKind op = TokenKind::Error;   // Unary, Binary
    ExprPtr lhs;                       // Unary uses lhs only
    ExprPtr rhs;

    // Filled in by the semantic analyser, read by the IR builder. It stays
    // Unknown until then, which is how we know the analyser actually ran.
    Type type = Type::Unknown;
};

enum class StmtKind {
    Declare,    // পূর্ণ ক = ১০।
    Assign,     // ক = ক + ১।
    Print,      // দেখাও(ক)।
    If,         // যদি (...) { ... } নাহলে { ... }
    While,      // যতক্ষণ (...) { ... }
    Block       // { ... }
};

struct Stmt {
    StmtKind kind;
    Token token;                  // the keyword or name that started the statement

    TokenKind declaredType = TokenKind::Error;   // Declare: KwPurno or KwDoshomik
    std::string name;                            // Declare, Assign

    ExprPtr value;                // Declare/Assign right-hand side, Print argument,
                                  // If/While condition
    std::vector<StmtPtr> body;      // Block contents, then-branch, loop body
    std::vector<StmtPtr> elseBody;  // If: the নাহলে branch
};

struct Program {
    std::vector<StmtPtr> statements;
};

// --emit=ast: prints the tree as indented text.
void printAst(const Program& program, std::ostream& out);

// Helper shared by the printer and later phases.
const char* exprKindName(ExprKind kind);
const char* stmtKindName(StmtKind kind);

} // namespace sutro
