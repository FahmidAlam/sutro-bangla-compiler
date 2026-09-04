// The typed three-address IR — the hand-off every backend reads.
//
// This is the phase the rest of the compiler hangs on: the Java backend, the
// Python backend and the optional WebAssembly target are all written against
// this one structure, so anything a backend would otherwise have to work out for
// itself has to be decided here instead. That single rule explains every choice
// below, and it is worth stating as a test: if an emitter ever needs an `if` that
// asks a question the IR does not already answer, the IR is wrong.
//
// Three consequences of that rule:
//
//   1. Every instruction carries its own result Type, and so does every operand.
//      That is redundant on purpose. Python's '/' is always float division, so an
//      int-typed Div must emit '//' — an emitter that had to derive the type from
//      its operands would be doing analysis, and a Python backend that got it
//      wrong would silently compute a different program than the Java one.
//   2. The analyser's Widen node becomes a real ToFloat instruction. An emitter
//      that meets ToFloat prints a cast; an emitter that never meets one never
//      casts. No emitter infers a conversion, because সূত্র has no narrowing
//      anywhere and int -> float is the only conversion in the language.
//   3. Control flow is *bracketed* — If/Else/EndIf and Loop/ExitIfFalse/EndLoop —
//      not labels and jumps. Textbook three-address code uses `goto L1`, but
//      Python has no goto, so a flat jump list would force the Python emitter to
//      rediscover the loops and branches the parser already knew in order to work
//      out where to indent. That is exactly the analysis this IR exists to
//      prevent. Brackets cost each emitter one indent counter, and they are also
//      what WebAssembly's structured block/loop/if wants, so the optional third
//      target stays cheap.
//
// The IR is a flat instruction list plus a flat table of slots. It is not a basic
// block graph, because there is deliberately no optimisation phase: generated code
// should stay recognisable to the student who wrote the Bangla.
#pragma once

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.hpp"
#include "diagnostics.hpp"
#include "type.hpp"

namespace sutro {

// A local in the generated program. User variables and compiler temporaries share
// one table because the backends declare them identically — Java needs a typed
// declaration for both at the top of main, and making an emitter hunt for
// temporaries by scanning the instruction stream is the sort of work this IR is
// supposed to have already done.
struct Slot {
    int id = 0;                  // its own index in IrProgram::slots
    Type type = Type::Unknown;   // Int, Float or Bool — never Unknown after build()
    bool temporary = false;      // false means the student wrote this name

    std::string name;            // as written: "ক", "গড়". Empty for a temporary.
    std::string emit;            // the identifier BOTH backends print — legal in
                                 // Java and in Python, and unique program-wide.

    int line = 0;                // where it was declared; 0 for a temporary
    int column = 0;
};

enum class OperandKind {
    None,        // this operand slot is unused by this opcode
    Slot,        // a variable or temporary  ->  IrProgram::slots[slot]
    IntConst,    // ১০   -> Java "10L",  Python "10"
    FloatConst   // ১.৫  -> Java "1.5",  Python "1.5"
};

// A leaf expression becomes an operand directly rather than getting an
// instruction of its own, which is what keeps `ক + ১` two operands and one
// opcode instead of a stack machine written sideways.
struct Operand {
    OperandKind kind = OperandKind::None;
    Type type = Type::Unknown;   // for a Slot this duplicates slots[slot].type,
                                 // deliberately: asking an operand its type must
                                 // never be a second lookup a backend could skip
    int slot = -1;               // Slot
    long long intValue = 0;      // IntConst
    double floatValue = 0.0;     // FloatConst
};

Operand slotRef(int id, Type t);
Operand intConst(long long v);
Operand floatConst(double v);

enum class Op {
    // dst <- a
    Move,        // copy; the only thing an assignment or declaration emits
    Neg,         // unary minus
    ToFloat,     // int -> float. The analyser's Widen, made executable.

    // dst <- a op b, dst typed Int or Float
    Add, Sub, Mul, Div,

    // dst <- a op b, dst typed Bool
    Lt, Gt, Le, Ge, Eq, Ne,

    Print,       // print a

    // Bracketed control flow. Each opening bracket has exactly one closer, and
    // they nest, so an emitter tracks depth with an int and never a stack.
    If,          // if (a) {          — a is a Bool operand
    Else,        // } else {
    EndIf,       // }
    Loop,        // while (true) {
    ExitIfFalse, // if (!a) break;    — a is a Bool operand
    EndLoop      // }
};

struct Instr {
    Op op = Op::Move;
    Type type = Type::Unknown;   // the result type; Unknown for brackets

    int dst = -1;                // slot index written, or -1. Deliberately a bare
                                 // int and not an Operand: a constant can never
                                 // be assigned to, and saying so costs nothing.
    Operand a;
    Operand b;

    int line = 0;                // back to the .sutro the student wrote, so a
    int column = 0;              // codegen or runtime message can point at it
};

struct IrProgram {
    std::vector<Slot> slots;
    std::vector<Instr> code;
};

const char* opName(Op op);

// A double written as a literal both targets accept and neither rounds.
std::string floatLiteral(double v);

// Lowers the typed AST. Only ever runs when the analyser reported no errors, so
// every Expr already carries a resolved Type and every name already resolves.
class IrBuilder {
public:
    explicit IrBuilder(DiagnosticBag& diagnostics);

    IrProgram build(const Program& program);

private:
    void statement(const Stmt* s);
    void block(const std::vector<StmtPtr>& body);

    // Lowers an expression and yields the operand holding its value, emitting
    // whatever instructions that takes.
    Operand value(const Expr* e);

    int newSlot(const std::string& name, Type type, int line, int column,
                bool temporary);
    int newTemp(Type type);

    // Picks the identifier both backends will print. Three hazards, all settled
    // here so no emitter has to think about names at all: সূত্র lets an inner
    // scope shadow an outer one but Java forbids one local shadowing another;
    // a সূত্র identifier may be plain ASCII, so `পূর্ণ class = ১।` is legal
    // সূত্র and illegal Java; and utf8::isIdentStart accepts the whole Bengali
    // block, including matras that neither Java nor Python allows in a name.
    std::string emitNameFor(const std::string& name);

    void pushScope();
    void popScope();
    int lookup(const std::string& name) const;

    void emit(Instr in);

    // Reports a broken compiler invariant rather than letting a malformed
    // instruction reach a backend. No phase throws at the user, so even an
    // internal fault goes through the same diagnostics bag as everything else.
    void internalError(const std::string& name, const Token& at);

    DiagnosticBag& diags_;
    IrProgram ir_;
    std::vector<std::unordered_map<std::string, int>> scopes_;
    std::unordered_set<std::string> taken_;   // every emit name handed out so far
    int tempCount_ = 0;
};

// --emit=ir
void printIr(const IrProgram& ir, std::ostream& out);

} // namespace sutro
