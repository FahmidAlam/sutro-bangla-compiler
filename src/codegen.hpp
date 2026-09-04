// The backends: IR in, a runnable source file out.
//
// Both emitters are a switch over Op and nothing else. Neither one infers a type,
// re-derives a conversion, looks at the AST, or reasons about control flow — every
// question it could ask was answered by the IR builder, which is the property the
// whole IR design exists to buy. If either function below ever grows an `if` that
// inspects something other than the instruction in front of it, the IR is wrong
// and the fix belongs in ir.cpp.
//
// They are free functions rather than an Emitter class hierarchy for the same
// reason the rest of the compiler switches on a `kind` tag: no virtual calls, and
// a reader can follow one function top to bottom.
//
// Where the two targets genuinely disagree, the IR is not what differs — the
// emitted text is:
//
//   পূর্ণ division  Java's `/` on long truncates toward zero. Python's `//` floors,
//                   so -৭ / ২ would be -3 in Java and -4 in Python. The Python
//                   backend emits a helper that truncates, so both agree.
//   empty block     `যদি (ক > ০) { }` is legal সূত্র. Java prints `{ }` happily;
//                   Python needs a `pass` or the file will not parse.
//   declarations    Java needs every local declared and definitely assigned before
//                   use, so slots are hoisted and zero-initialised. Python needs
//                   none of that, and emitting them anyway would be noise.
#pragma once

#include <iosfwd>
#include <string>

#include "ir.hpp"

namespace sutro
{

    // Added by Swadheen Islam Robi
    // Writes a complete .py file. `sourcePath` only appears in the header comment.
    void emitPython(const IrProgram &ir, const std::string &sourcePath,
                    std::ostream &out);

    // Writes a complete .java file. `className` must match the file's own name or
    // javac refuses it, so the driver derives it from the output path.
    void emitJava(const IrProgram &ir, const std::string &sourcePath,
                  const std::string &className, std::ostream &out);

} // namespace sutro
