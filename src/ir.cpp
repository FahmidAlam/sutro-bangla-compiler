#include "ir.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>

#include "utf8.hpp"

namespace sutro {

Operand slotRef(int id, Type t) {
    Operand o;
    o.kind = OperandKind::Slot;
    o.slot = id;
    o.type = t;
    return o;
}

Operand intConst(long long v) {
    Operand o;
    o.kind = OperandKind::IntConst;
    o.intValue = v;
    o.type = Type::Int;
    return o;
}

Operand floatConst(double v) {
    Operand o;
    o.kind = OperandKind::FloatConst;
    o.floatValue = v;
    o.type = Type::Float;
    return o;
}

// The operator as a student wrote it. The dump reads as arithmetic rather than as
// a list of enumerator names, which matters because --emit=ir is what a team member
// puts on screen next to the Bangla source at a review.
const char* opSymbol(Op op) {
    switch (op) {
        case Op::Add: return "+";
        case Op::Sub: return "-";
        case Op::Mul: return "*";
        case Op::Div: return "/";
        case Op::Lt:  return "<";
        case Op::Gt:  return ">";
        case Op::Le:  return "<=";
        case Op::Ge:  return ">=";
        case Op::Eq:  return "==";
        case Op::Ne:  return "!=";
        default:      return "?";
    }
}

const char* opName(Op op) {
    switch (op) {
        case Op::Move:        return "Move";
        case Op::Neg:         return "Neg";
        case Op::ToFloat:     return "ToFloat";
        case Op::Add:         return "Add";
        case Op::Sub:         return "Sub";
        case Op::Mul:         return "Mul";
        case Op::Div:         return "Div";
        case Op::Lt:          return "Lt";
        case Op::Gt:          return "Gt";
        case Op::Le:          return "Le";
        case Op::Ge:          return "Ge";
        case Op::Eq:          return "Eq";
        case Op::Ne:          return "Ne";
        case Op::Print:       return "Print";
        case Op::If:          return "If";
        case Op::Else:        return "Else";
        case Op::EndIf:       return "EndIf";
        case Op::Loop:        return "Loop";
        case Op::ExitIfFalse: return "ExitIfFalse";
        case Op::EndLoop:     return "EndLoop";
    }
    return "?";
}

namespace {

Op binaryOp(TokenKind k) {
    switch (k) {
        case TokenKind::Plus:         return Op::Add;
        case TokenKind::Minus:        return Op::Sub;
        case TokenKind::Star:         return Op::Mul;
        case TokenKind::Slash:        return Op::Div;
        case TokenKind::Less:         return Op::Lt;
        case TokenKind::Greater:      return Op::Gt;
        case TokenKind::LessEqual:    return Op::Le;
        case TokenKind::GreaterEqual: return Op::Ge;
        case TokenKind::EqualEqual:   return Op::Eq;
        case TokenKind::BangEqual:    return Op::Ne;
        default:                      return Op::Move;   // unreachable after sema
    }
}

// A Bengali letter proper, as opposed to everything utf8::isIdentStart lets into
// a সূত্র name. The lexer accepts the entire Bengali block minus the digits,
// which sweeps in matras, হসন্ত and the currency signs — legal to start a সূত্র
// identifier with, rejected by both Java and Python.
bool isTargetNameStart(char32_t cp) {
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_') return true;
    return (cp >= 0x0985 && cp <= 0x09B9)    // অ..হ
        || (cp >= 0x09DC && cp <= 0x09DF)    // ড়, ঢ়, য়
        ||  cp == 0x09CE                     // ৎ
        || (cp >= 0x09F0 && cp <= 0x09F1);   // ৰ, ৱ
}

// Java's isJavaIdentifierPart and Python's XID_Continue both accept combining
// marks and digits, so a matra is fine anywhere except first.
bool isTargetNameContinue(char32_t cp) {
    return isTargetNameStart(cp)
        || utf8::isAsciiDigit(cp)
        || utf8::isBanglaDigit(cp)
        || utf8::isCombiningMark(cp);
}

// The union of both targets' reserved words, plus the handful of names the
// emitters themselves put in scope. Taking the union rather than one set per
// backend is what lets Java and Python print byte-identical variable names, which
// in turn means a student comparing the two outputs sees only the syntax differ.
const std::unordered_set<std::string>& reservedWords() {
    static const std::unordered_set<std::string> words = {
        // Java
        "abstract", "assert", "boolean", "break", "byte", "case", "catch", "char",
        "class", "const", "continue", "default", "do", "double", "else", "enum",
        "extends", "final", "finally", "float", "for", "goto", "if", "implements",
        "import", "instanceof", "int", "interface", "long", "native", "new",
        "package", "private", "protected", "public", "return", "short", "static",
        "strictfp", "super", "switch", "synchronized", "this", "throw", "throws",
        "transient", "try", "void", "volatile", "while",
        "true", "false", "null", "var", "record", "sealed", "permits", "yield",
        // Python
        "and", "as", "async", "await", "def", "del", "elif", "except", "from",
        "global", "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
        "with", "None", "True", "False", "match",
        // names the emitters introduce
        "System", "String", "Math", "Object", "args", "main", "Program",
        "print", "str", "bool", "abs", "_shutro_div",
    };
    return words;
}

} // namespace

IrBuilder::IrBuilder(DiagnosticBag& diagnostics) : diags_(diagnostics) {}

void IrBuilder::internalError(const std::string& name, const Token& at) {
    diags_.error("I01", name + " এর জন্য কোনো ঘর পাওয়া যায়নি",
                 "এটি কম্পাইলারের নিজের ত্রুটি — অনুগ্রহ করে জানান",
                 at.line, at.column, at.length);
}

void IrBuilder::pushScope() {
    scopes_.emplace_back();
}

void IrBuilder::popScope() {
    if (scopes_.size() > 1) scopes_.pop_back();
}

int IrBuilder::lookup(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        const auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return -1;
}

std::string IrBuilder::emitNameFor(const std::string& name) {
    std::string base;
    bool first = true;

    for (std::size_t i = 0; i < name.size();) {
        const auto d = utf8::decode(name, i);
        i += static_cast<std::size_t>(d.bytes);

        const bool ok = first ? isTargetNameStart(d.cp) : isTargetNameContinue(d.cp);
        if (ok) {
            base += utf8::encode(d.cp);
        } else {
            // Spell the codepoint out rather than dropping it, so two names that
            // differ only in an illegal character cannot collapse into one.
            static const char* kHex = "0123456789abcdef";
            base += "_u";
            for (int shift = 12; shift >= 0; shift -= 4) {
                base += kHex[(static_cast<unsigned>(d.cp) >> shift) & 0xF];
            }
        }
        first = false;
    }

    if (base.empty()) base = "v";
    if (!isTargetNameStart(utf8::decode(base, 0).cp)) base = "v_" + base;

    // Shadowing is legal in সূত্র and illegal between two Java locals, and every
    // slot is hoisted into one method anyway, so colliding names are numbered.
    std::string candidate = base;
    int n = 2;
    while (reservedWords().count(candidate) != 0 || taken_.count(candidate) != 0) {
        candidate = base + "_" + std::to_string(n++);
    }
    taken_.insert(candidate);
    return candidate;
}

int IrBuilder::newSlot(const std::string& name, Type type, int line, int column,
                       bool temporary) {
    Slot s;
    s.id = static_cast<int>(ir_.slots.size());
    s.type = type;
    s.temporary = temporary;
    s.name = name;
    s.emit = emitNameFor(name.empty() ? "t" + std::to_string(tempCount_) : name);
    s.line = line;
    s.column = column;
    ir_.slots.push_back(s);
    return s.id;
}

int IrBuilder::newTemp(Type type) {
    const int id = newSlot(std::string(), type, 0, 0, true);
    ++tempCount_;
    return id;
}

void IrBuilder::emit(Instr in) {
    ir_.code.push_back(in);
}

IrProgram IrBuilder::build(const Program& program) {
    pushScope();
    for (const StmtPtr& s : program.statements) statement(s.get());
    return std::move(ir_);
}

void IrBuilder::block(const std::vector<StmtPtr>& body) {
    for (const StmtPtr& s : body) statement(s.get());
}

void IrBuilder::statement(const Stmt* s) {
    if (s == nullptr) return;

    switch (s->kind) {
        case StmtKind::Declare: {
            // The initialiser is lowered before the name exists, matching the
            // analyser: পূর্ণ ক = ক + ১। is a T01 error there, so by the time the
            // IR runs the right-hand side can never mean the slot being created.
            const Operand v = value(s->value.get());
            const Type t = s->declaredType == TokenKind::KwDoshomik ? Type::Float
                                                                   : Type::Int;
            const int slot = newSlot(s->name, t, s->nameToken.line,
                                     s->nameToken.column, false);
            scopes_.back()[s->name] = slot;

            Instr in;
            in.op = Op::Move;
            in.type = t;
            in.dst = slot;
            in.a = v;
            in.line = s->nameToken.line;
            in.column = s->nameToken.column;
            emit(in);
            break;
        }

        case StmtKind::Assign: {
            const Operand v = value(s->value.get());
            const int slot = lookup(s->name);
            if (slot < 0) {
                // Unreachable: the analyser resolves every name before we run. If
                // it ever fires, something upstream changed, and saying so loudly
                // beats emitting an instruction with no destination that a backend
                // would then turn into malformed Java.
                internalError(s->name, s->nameToken);
                return;
            }

            Instr in;
            in.op = Op::Move;
            in.type = ir_.slots[static_cast<std::size_t>(slot)].type;
            in.dst = slot;
            in.a = v;
            in.line = s->nameToken.line;
            in.column = s->nameToken.column;
            emit(in);
            break;
        }

        case StmtKind::Print: {
            const Operand v = value(s->value.get());
            Instr in;
            in.op = Op::Print;
            in.type = v.type;        // the backend prints by type, never by guess
            in.a = v;
            in.line = s->token.line;
            in.column = s->token.column;
            emit(in);
            break;
        }

        case StmtKind::If: {
            const Operand cond = value(s->value.get());

            Instr open;
            open.op = Op::If;
            open.a = cond;
            open.line = s->token.line;
            open.column = s->token.column;
            emit(open);

            pushScope();
            block(s->body);
            popScope();

            if (!s->elseBody.empty()) {
                Instr other;
                other.op = Op::Else;
                other.line = s->token.line;
                other.column = s->token.column;
                emit(other);

                pushScope();
                block(s->elseBody);
                popScope();
            }

            Instr close;
            close.op = Op::EndIf;
            close.line = s->token.line;
            close.column = s->token.column;
            emit(close);
            break;
        }

        case StmtKind::While: {
            // Loop comes first and the condition is lowered *inside* it. That
            // ordering is the whole loop: the comparison and every instruction
            // that feeds it have to re-run each iteration, and emitting them
            // before Loop would evaluate the condition once and spin forever.
            Instr open;
            open.op = Op::Loop;
            open.line = s->token.line;
            open.column = s->token.column;
            emit(open);

            const Operand cond = value(s->value.get());

            Instr test;
            test.op = Op::ExitIfFalse;
            test.a = cond;
            test.line = s->token.line;
            test.column = s->token.column;
            emit(test);

            pushScope();
            block(s->body);
            popScope();

            Instr close;
            close.op = Op::EndLoop;
            close.line = s->token.line;
            close.column = s->token.column;
            emit(close);
            break;
        }

        case StmtKind::Block:
            pushScope();
            block(s->body);
            popScope();
            break;
    }
}

Operand IrBuilder::value(const Expr* e) {
    if (e == nullptr) return Operand{};

    switch (e->kind) {
        case ExprKind::IntLit:
            return intConst(e->intValue);

        case ExprKind::FloatLit:
            return floatConst(e->floatValue);

        case ExprKind::Name: {
            const int slot = lookup(e->name);
            if (slot < 0) {                   // unreachable — see the Assign case
                internalError(e->name, e->token);
                return Operand{};
            }
            return slotRef(slot, ir_.slots[static_cast<std::size_t>(slot)].type);
        }

        case ExprKind::Unary: {
            const Operand a = value(e->lhs.get());
            const int dst = newTemp(e->type);

            Instr in;
            in.op = Op::Neg;
            in.type = e->type;
            in.dst = dst;
            in.a = a;
            in.line = e->token.line;
            in.column = e->token.column;
            emit(in);
            return slotRef(dst, e->type);
        }

        case ExprKind::Widen: {
            const Operand a = value(e->lhs.get());
            const int dst = newTemp(Type::Float);

            Instr in;
            in.op = Op::ToFloat;
            in.type = Type::Float;
            in.dst = dst;
            in.a = a;
            in.line = e->token.line;
            in.column = e->token.column;
            emit(in);
            return slotRef(dst, Type::Float);
        }

        case ExprKind::Binary: {
            const Operand a = value(e->lhs.get());
            const Operand b = value(e->rhs.get());
            const int dst = newTemp(e->type);

            Instr in;
            in.op = binaryOp(e->op);
            in.type = e->type;
            in.dst = dst;
            in.a = a;
            in.b = b;
            in.line = e->token.line;
            in.column = e->token.column;
            emit(in);
            return slotRef(dst, e->type);
        }
    }

    return Operand{};
}

namespace {

} // namespace

// std::to_string(0.0) is "0.000000". Both backends need a real source literal and
// so does the dump, so the formatting lives in one place: shortest round-trippable
// form, with a ".0" forced on so a float never reads as an int.
std::string floatLiteral(double v) {
    std::ostringstream os;
    os << std::setprecision(17) << v;
    std::string t = os.str();
    if (t.find('.') == std::string::npos && t.find('e') == std::string::npos
        && t.find("inf") == std::string::npos && t.find("nan") == std::string::npos) {
        t += ".0";
    }
    return t;
}

namespace {

std::string describeOperand(const IrProgram& ir, const Operand& o) {
    switch (o.kind) {
        case OperandKind::None:       return "-";
        case OperandKind::IntConst:   return std::to_string(o.intValue);
        case OperandKind::FloatConst: return floatLiteral(o.floatValue);
        // The emit name alone: it is unique program-wide, it is exactly what the
        // backend will print, and showing a slot index beside it only invites
        // confusing two numbering schemes for one thing.
        case OperandKind::Slot:
            if (o.slot < 0 || static_cast<std::size_t>(o.slot) >= ir.slots.size()) return "?";
            return ir.slots[static_cast<std::size_t>(o.slot)].emit;
    }
    return "?";
}

} // namespace

void printIr(const IrProgram& ir, std::ostream& out) {
    // Positions stay in ASCII digits like --emit=tokens and --emit=sym: the dumps
    // are a developer tool, and Bangla digits belong to the diagnostics a student
    // actually reads.
    out << "slots:\n";
    if (ir.slots.empty()) out << "  (none)\n";
    for (const Slot& s : ir.slots) {
        const std::string id = (s.temporary ? "t" : "s") + std::to_string(s.id);
        const std::string where = s.temporary
            ? std::string("-")
            : std::to_string(s.line) + ":" + std::to_string(s.column);
        out << "  " << utf8::padTo(id, 6)
            << utf8::padTo(s.temporary ? "(temp)" : s.name, 14)
            << utf8::padTo(s.emit, 14)
            << utf8::padTo(typeName(s.type), 10)
            << where << "\n";
    }

    out << "\ncode:\n";
    if (ir.code.empty()) out << "  (none)\n";

    int indent = 0;
    for (std::size_t i = 0; i < ir.code.size(); ++i) {
        const Instr& in = ir.code[i];
        // Brackets are balanced by construction, so this cannot go negative — but
        // it feeds a std::string(n, ' ') below, where a negative would become a
        // gigantic allocation, and §2.1 forbids crashing on any input at all.
        if (in.op == Op::Else || in.op == Op::EndIf || in.op == Op::EndLoop) --indent;
        if (indent < 0) indent = 0;

        std::string text;
        switch (in.op) {
            case Op::Move:
                text = describeOperand(ir, slotRef(in.dst, in.type)) + " = "
                     + describeOperand(ir, in.a);
                break;
            case Op::Neg:
            case Op::ToFloat:
                text = describeOperand(ir, slotRef(in.dst, in.type)) + " = "
                     + opName(in.op) + " " + describeOperand(ir, in.a);
                break;
            case Op::If:
            case Op::ExitIfFalse:
                text = std::string(opName(in.op)) + " " + describeOperand(ir, in.a);
                break;
            case Op::Print:
                text = "Print " + describeOperand(ir, in.a);
                break;
            case Op::Else:
            case Op::EndIf:
            case Op::Loop:
            case Op::EndLoop:
                text = opName(in.op);
                break;
            default:
                text = describeOperand(ir, slotRef(in.dst, in.type)) + " = "
                     + describeOperand(ir, in.a) + " " + opSymbol(in.op) + " "
                     + describeOperand(ir, in.b);
                break;
        }

        std::string row = "  " + utf8::padTo(std::to_string(i), 5)
                        + std::string(static_cast<std::size_t>(indent) * 2, ' ')
                        + text;
        out << utf8::padTo(row, 56);
        if (in.type != Type::Unknown) out << ": " << utf8::padTo(typeName(in.type), 8);
        else                          out << utf8::padTo("", 10);
        out << " ; " << in.line << ":" << in.column << "\n";

        if (in.op == Op::If || in.op == Op::Else || in.op == Op::Loop) ++indent;
    }
}

} // namespace sutro
