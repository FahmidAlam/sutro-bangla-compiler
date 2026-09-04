#include "codegen.hpp"

#include <ostream>
#include <string>
#include <vector>

namespace sutro
{

    namespace
    {
        // Added by Swadheen Islam Robi
        // Truncating integer division, spelled out because Python's // does not do it.
        // Java's `/` on long rounds toward zero; Python's `//` floors, so -৭ / ২ is -3 in
        // one target and -4 in the other. সূত্র picks Java's answer — a student reading
        // `-৭ / ২` expects the fraction dropped, not the value pushed away from zero —
        // and this helper is how Python gets there. It is emitted only when the program
        // actually contains an integer division.
        const char *kDivHelper =
            "def _shutro_div(a, b):\n"
            "    # পূর্ণ ভাগ শূন্যের দিকে কাটা হয়, Python-এর // এর মতো নিচের দিকে নয়।\n"
            "    q = abs(a) // abs(b)\n"
            "    return -q if (a < 0) != (b < 0) else q\n";

        bool needsDivHelper(const IrProgram &ir)
        {
            for (const Instr &in : ir.code)
            {
                if (in.op == Op::Div && in.type == Type::Int)
                    return true;
            }
            return false;
        }

        std::string operand(const IrProgram &ir, const Operand &o)
        {
            switch (o.kind)
            {
            case OperandKind::None:
                return "None";
            case OperandKind::IntConst:
                return std::to_string(o.intValue);
            case OperandKind::FloatConst:
                return floatLiteral(o.floatValue);
            case OperandKind::Slot:
                return ir.slots[static_cast<std::size_t>(o.slot)].emit;
            }
            return "None";
        }

        const char *pyBinary(Op op)
        {
            switch (op)
            {
            case Op::Add:
                return " + ";
            case Op::Sub:
                return " - ";
            case Op::Mul:
                return " * ";
            case Op::Div:
                return " / "; // float division only; Int goes via helper
            case Op::Lt:
                return " < ";
            case Op::Gt:
                return " > ";
            case Op::Le:
                return " <= ";
            case Op::Ge:
                return " >= ";
            case Op::Eq:
                return " == ";
            case Op::Ne:
                return " != ";
            default:
                return " ? ";
            }
        }

    } // namespace

    void emitPython(const IrProgram &ir, const std::string &sourcePath,
                    std::ostream &out)
    {
        out << "# " << sourcePath << " থেকে সূত্র কম্পাইলার দিয়ে তৈরি।\n"
            << "# This file is generated. Edit the .sutro source instead.\n\n";

        if (needsDivHelper(ir))
            out << kDivHelper << "\n";

        // No declarations: Python does not need them, and the IR is in execution
        // order, so every slot is assigned before it is ever read.

        int indent = 1; // module level is 1 so an empty program still emits cleanly
        // One flag per open block, answering "did anything land inside it?". সূত্র
        // allows `যদি (ক > ০) { }` and Python has no empty suite, so a block that
        // stayed empty gets a `pass`.
        std::vector<bool> filled;

        const auto pad = [&out](int depth)
        {
            for (int i = 1; i < depth; ++i)
                out << "    ";
        };
        const auto mark = [&filled]()
        {
            if (!filled.empty())
                filled.back() = true;
        };
        const auto closeBlock = [&](int depth)
        {
            if (!filled.empty() && !filled.back())
            {
                pad(depth);
                out << "pass\n";
            }
            if (!filled.empty())
                filled.pop_back();
        };

        for (const Instr &in : ir.code)
        {
            switch (in.op)
            {
            case Op::Else:
                closeBlock(indent);
                --indent;
                pad(indent);
                out << "else:\n";
                ++indent;
                filled.push_back(false);
                continue;

            case Op::EndIf:
            case Op::EndLoop:
                closeBlock(indent);
                --indent;
                continue;

            default:
                break;
            }

            mark();
            pad(indent);

            switch (in.op)
            {
            case Op::Move:
                out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                    << " = " << operand(ir, in.a) << "\n";
                break;

            case Op::Neg:
                out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                    << " = -" << operand(ir, in.a) << "\n";
                break;

            case Op::ToFloat:
                out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                    << " = float(" << operand(ir, in.a) << ")\n";
                break;

            case Op::Div:
                // The one place the emitter reads in.type — and it reads it, it
                // does not work it out. An int-typed Div must not become `/`.
                if (in.type == Type::Int)
                {
                    out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                        << " = _shutro_div(" << operand(ir, in.a) << ", "
                        << operand(ir, in.b) << ")\n";
                }
                else
                {
                    out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                        << " = " << operand(ir, in.a) << " / "
                        << operand(ir, in.b) << "\n";
                }
                break;

            case Op::Print:
                out << "print(" << operand(ir, in.a) << ")\n";
                break;

            case Op::If:
                out << "if " << operand(ir, in.a) << ":\n";
                ++indent;
                filled.push_back(false);
                break;

            case Op::Loop:
                out << "while True:\n";
                ++indent;
                filled.push_back(false);
                break;

            case Op::ExitIfFalse:
                out << "if not " << operand(ir, in.a) << ":\n";
                pad(indent + 1);
                out << "break\n";
                break;

            default:
                out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                    << " = " << operand(ir, in.a) << pyBinary(in.op)
                    << operand(ir, in.b) << "\n";
                break;
            }
        }

        // A program that is only declarations still has to be a valid module.
        if (ir.code.empty())
            out << "pass\n";
    }

} // namespace sutro
