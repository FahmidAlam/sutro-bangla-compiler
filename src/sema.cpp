#include "sema.hpp"

#include <utility>

#include "utf8.hpp"

namespace sutro {

namespace {

Type typeFromKeyword(TokenKind kind) {
    switch (kind) {
        case TokenKind::KwPurno:    return Type::Int;
        case TokenKind::KwDoshomik: return Type::Float;
        default:                    return Type::Error;
    }
}

bool isComparison(TokenKind op) {
    switch (op) {
        case TokenKind::Less:
        case TokenKind::Greater:
        case TokenKind::LessEqual:
        case TokenKind::GreaterEqual:
        case TokenKind::EqualEqual:
        case TokenKind::BangEqual:
            return true;
        default:
            return false;
    }
}

// True for a literal zero written directly as the divisor. Only a literal is
// checked: proving a variable is zero is an optimiser's job, and সূত্র has no
// optimiser by design.
bool isLiteralZero(const Expr* e) {
    if (e == nullptr) return false;
    if (e->kind == ExprKind::Widen)    return isLiteralZero(e->lhs.get());
    if (e->kind == ExprKind::IntLit)   return e->intValue == 0;
    if (e->kind == ExprKind::FloatLit) return e->floatValue == 0.0;
    return false;
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(DiagnosticBag& diagnostics)
    : diags_(diagnostics) {}

void SemanticAnalyzer::widen(ExprPtr& e) {
    auto wrapped = std::make_unique<Expr>();
    wrapped->kind = ExprKind::Widen;
    wrapped->token = e->token;      // the conversion points at the value it converts
    wrapped->type = Type::Float;
    wrapped->lhs = std::move(e);
    e = std::move(wrapped);
}

void SemanticAnalyzer::analyze(Program& program) {
    for (StmtPtr& s : program.statements) statement(s.get());
    warnUnused();
}

void SemanticAnalyzer::block(std::vector<StmtPtr>& body) {
    for (StmtPtr& s : body) statement(s.get());
}

void SemanticAnalyzer::statement(Stmt* s) {
    if (s == nullptr) return;       // a statement the parser could not build

    switch (s->kind) {
        case StmtKind::Declare:
            declareStmt(*s);
            break;

        case StmtKind::Assign:
            assignStmt(*s);
            break;

        case StmtKind::Print: {
            const Type t = expression(s->value);
            if (t == Type::Bool) {
                diags_.error("T05", "তুলনার ফলাফল দেখানো যায় না",
                             "দেখাও শুধু একটি সংখ্যা নেয়",
                             s->token.line, s->token.column, s->token.length);
            }
            break;
        }

        case StmtKind::If:
            conditionOf(*s);
            // Each branch is its own scope, so a name declared inside one is not
            // visible in the other, nor after the যদি ends.
            symbols_.pushScope();
            block(s->body);
            symbols_.popScope();
            symbols_.pushScope();
            block(s->elseBody);
            symbols_.popScope();
            break;

        case StmtKind::While:
            conditionOf(*s);
            symbols_.pushScope();
            block(s->body);
            symbols_.popScope();
            break;

        case StmtKind::Block:
            symbols_.pushScope();
            block(s->body);
            symbols_.popScope();
            break;
    }
}

void SemanticAnalyzer::declareStmt(Stmt& s) {
    const Type declared = typeFromKeyword(s.declaredType);

    // The initialiser is analysed *before* the name enters the table, so
    // পূর্ণ ক = ক + ১। reports that ক is undeclared instead of quietly reading
    // the variable it is in the middle of creating.
    expression(s.value);

    const Token& at = s.nameToken;
    if (Symbol* existing = symbols_.lookupCurrentScope(s.name)) {
        diags_.error("T02", "এই স্কোপে " + s.name + " আগেই ঘোষণা করা হয়েছে",
                     "আগের ঘোষণা " + utf8::toBanglaDigits(existing->line)
                         + " নম্বর লাইনে — নতুন নাম দিন, অথবা শুধু মান বদলান",
                     at.line, at.column, at.length);
        // Keep checking against the type already on record, so the rest of the
        // statement still produces useful messages.
        checkAssignable(existing->type, s.value, at, s.name);
        return;
    }

    symbols_.declare(s.name, declared, at.line, at.column);
    checkAssignable(declared, s.value, at, s.name);
}

void SemanticAnalyzer::assignStmt(Stmt& s) {
    const Token& at = s.nameToken;
    Symbol* sym = symbols_.lookup(s.name);

    // Analyse the value either way: an undeclared target is no reason to hide
    // the mistakes on the right-hand side.
    expression(s.value);

    if (sym == nullptr) {
        diags_.error("T01", s.name + " ঘোষণা করা হয়নি",
                     "ব্যবহারের আগে লিখুন: পূর্ণ " + s.name + " = ০।",
                     at.line, at.column, at.length);
        return;
    }

    checkAssignable(sym->type, s.value, at, s.name);
}

void SemanticAnalyzer::conditionOf(Stmt& s) {
    const Type t = expression(s.value);
    // The grammar already refuses a condition without a comparison (P10), so this
    // can only fire if that rule is ever loosened. It is here so such a change
    // fails loudly instead of reaching a backend as a non-boolean if().
    if (t != Type::Bool && t != Type::Error) {
        diags_.error("T06", "শর্তটি একটি তুলনা নয়",
                     "যেমন: " + s.token.lexeme + " (ক > ০) { ... }",
                     s.token.line, s.token.column, s.token.length);
    }
}

Type SemanticAnalyzer::expression(ExprPtr& e) {
    if (!e) return Type::Error;

    if (depth_ >= kMaxExprDepth) {
        if (!depthReported_) {
            diags_.error("T07", "হিসাবটি অনেক জটিল",
                         "একটি হিসাবে সর্বোচ্চ "
                             + utf8::toBanglaDigits(kMaxExprDepth)
                             + " ধাপ লেখা যায় — হিসাবটি ভেঙে কয়েক লাইনে লিখুন",
                         e->token.line, e->token.column, e->token.length);
            depthReported_ = true;
        }
        e->type = Type::Error;
        return Type::Error;
    }

    // Decremented on every path out, including the early returns above having
    // already skipped the increment.
    ++depth_;
    struct Leave {
        int& d;
        ~Leave() { --d; }
    } leave{depth_};

    switch (e->kind) {
        case ExprKind::IntLit:
            e->type = Type::Int;
            break;

        case ExprKind::FloatLit:
            e->type = Type::Float;
            break;

        case ExprKind::Name: {
            Symbol* sym = symbols_.lookup(e->name);
            if (sym == nullptr) {
                diags_.error("T01", e->name + " ঘোষণা করা হয়নি",
                             "ব্যবহারের আগে লিখুন: পূর্ণ " + e->name + " = ০।",
                             e->token.line, e->token.column, e->token.length);
                e->type = Type::Error;
            } else {
                ++sym->uses;
                e->type = sym->type;
            }
            break;
        }

        case ExprKind::Unary: {
            const Type t = expression(e->lhs);
            e->type = (t == Type::Int || t == Type::Float) ? t : Type::Error;
            break;
        }

        case ExprKind::Binary:
            e->type = binary(*e);
            break;

        case ExprKind::Widen:
            // Only this phase builds these, and it never runs twice over a tree.
            e->type = Type::Float;
            break;
    }

    return e->type;
}

Type SemanticAnalyzer::binary(Expr& e) {
    const Type lt = expression(e.lhs);
    const Type rt = expression(e.rhs);

    if (lt == Type::Error || rt == Type::Error) return Type::Error;

    if (lt == Type::Bool || rt == Type::Bool) {
        diags_.error("T04", "তুলনার ফলাফল দিয়ে আবার হিসাব করা যায় না",
                     "একটি শর্তে একটিই তুলনা লিখুন",
                     e.token.line, e.token.column, e.token.length);
        return Type::Error;
    }

    if (e.op == TokenKind::Slash && isLiteralZero(e.rhs.get())) {
        diags_.warning("W01", "শূন্য দিয়ে ভাগ করা হচ্ছে",
                       "চালানোর সময় প্রোগ্রামটি ভেঙে পড়বে",
                       e.token.line, e.token.column, e.token.length);
    }

    // Mixed arithmetic: the int side is converted here, once, in the open.
    if (lt == Type::Int && rt == Type::Float) widen(e.lhs);
    if (lt == Type::Float && rt == Type::Int) widen(e.rhs);

    if (isComparison(e.op)) return Type::Bool;

    return (lt == Type::Float || rt == Type::Float) ? Type::Float : Type::Int;
}

void SemanticAnalyzer::checkAssignable(Type target, ExprPtr& value, const Token& at,
                                       const std::string& name) {
    const Type vt = value ? value->type : Type::Error;

    // Unknown means the value never got analysed, which only happens after an
    // earlier error. Error means we have already complained about it.
    if (target == Type::Error || vt == Type::Error || vt == Type::Unknown) return;

    if (vt == Type::Bool) {
        diags_.error("T04", "তুলনার ফলাফল কোনো ঘরে রাখা যায় না",
                     "সূত্রতে সত্য/মিথ্যা রাখার কোনো টাইপ নেই",
                     at.line, at.column, at.length);
        return;
    }

    if (target == vt) return;

    if (target == Type::Float && vt == Type::Int) {
        widen(value);       // পূর্ণ into দশমিক loses nothing, so it happens silently
        return;
    }

    // দশমিক into পূর্ণ: the fraction would vanish. সূত্র makes you say so yourself.
    diags_.error("T03", name + " এর টাইপ " + typeName(target)
                     + ", কিন্তু মানটি " + typeName(vt),
                 "দশমিক অংশ হারিয়ে যাবে বলে এটি নিজে থেকে হয় না — "
                     + name + " কে দশমিক করে নিন",
                 at.line, at.column, at.length);
}

void SemanticAnalyzer::warnUnused() {
    for (const Symbol& s : symbols_.all()) {
        if (s.uses > 0) continue;
        diags_.warning("W02", s.name + " ঘোষণা করা হয়েছে কিন্তু কোথাও ব্যবহার হয়নি",
                       "নামটি মুছে ফেলুন, অথবা কোথাও ব্যবহার করুন",
                       s.line, s.column, static_cast<int>(utf8::visibleWidth(s.name)));
    }
}

} // namespace sutro
