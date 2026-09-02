#include "ast.hpp"

#include <ostream>

namespace sutro {

const char* exprKindName(ExprKind kind) {
    switch (kind) {
        case ExprKind::IntLit:   return "IntLit";
        case ExprKind::FloatLit: return "FloatLit";
        case ExprKind::Name:     return "Name";
        case ExprKind::Unary:    return "Unary";
        case ExprKind::Binary:   return "Binary";
    }
    return "?";
}

const char* stmtKindName(StmtKind kind) {
    switch (kind) {
        case StmtKind::Declare: return "Declare";
        case StmtKind::Assign:  return "Assign";
        case StmtKind::Print:   return "Print";
        case StmtKind::If:      return "If";
        case StmtKind::While:   return "While";
        case StmtKind::Block:   return "Block";
    }
    return "?";
}

namespace {

void indent(std::ostream& out, int depth) {
    for (int i = 0; i < depth; ++i) out << "  ";
}

void printExpr(const Expr* e, std::ostream& out, int depth) {
    if (e == nullptr) {
        indent(out, depth);
        out << "<missing>\n";
        return;
    }

    indent(out, depth);
    out << exprKindName(e->kind);

    switch (e->kind) {
        case ExprKind::IntLit:   out << " " << e->intValue;   break;
        case ExprKind::FloatLit: out << " " << e->floatValue; break;
        case ExprKind::Name:     out << " " << e->name;       break;
        case ExprKind::Unary:
        case ExprKind::Binary:   out << " " << e->token.lexeme; break;
    }

    // Empty until the semantic analyser runs, which makes it obvious in the dump
    // whether types have been decided yet.
    if (e->type != Type::Unknown) out << " : " << typeName(e->type);
    out << "\n";

    if (e->lhs) printExpr(e->lhs.get(), out, depth + 1);
    if (e->rhs) printExpr(e->rhs.get(), out, depth + 1);
}

void printStmt(const Stmt* s, std::ostream& out, int depth);

void printBody(const std::vector<StmtPtr>& body, std::ostream& out, int depth,
               const char* label) {
    indent(out, depth);
    out << label << "\n";
    for (const StmtPtr& child : body) printStmt(child.get(), out, depth + 1);
}

void printStmt(const Stmt* s, std::ostream& out, int depth) {
    if (s == nullptr) return;   // a statement that failed to parse

    indent(out, depth);
    out << stmtKindName(s->kind);

    if (s->kind == StmtKind::Declare) {
        out << " " << describe(s->declaredType) << " " << s->name;
    } else if (s->kind == StmtKind::Assign) {
        out << " " << s->name;
    }
    out << "\n";

    switch (s->kind) {
        case StmtKind::Declare:
        case StmtKind::Assign:
        case StmtKind::Print:
            printExpr(s->value.get(), out, depth + 1);
            break;

        case StmtKind::If:
            printBody({}, out, depth + 1, "condition");
            printExpr(s->value.get(), out, depth + 2);
            printBody(s->body, out, depth + 1, "then");
            if (!s->elseBody.empty()) printBody(s->elseBody, out, depth + 1, "else");
            break;

        case StmtKind::While:
            printBody({}, out, depth + 1, "condition");
            printExpr(s->value.get(), out, depth + 2);
            printBody(s->body, out, depth + 1, "body");
            break;

        case StmtKind::Block:
            for (const StmtPtr& child : s->body) printStmt(child.get(), out, depth + 1);
            break;
    }
}

} // namespace

void printAst(const Program& program, std::ostream& out) {
    out << "Program\n";
    for (const StmtPtr& s : program.statements) printStmt(s.get(), out, 1);
}

} // namespace sutro
