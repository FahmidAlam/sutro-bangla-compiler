#include "codegen.hpp"

#include <ostream>
#include <string>

namespace sutro
{

    namespace
    {
        // Added by Swadheen Islam Robi
        // পূর্ণ is documented as 64-bit, so it is `long` and every integer literal carries
        // the L suffix — without it, ৯২২৩৩৭২০৩৬৮৫৪৭৭৫৮০৭ would be an int literal and
        // javac would reject the file outright.
        const char *javaType(Type t)
        {
            switch (t)
            {
            case Type::Int:
                return "long";
            case Type::Float:
                return "double";
            case Type::Bool:
                return "boolean";
            default:
                return "long";
            }
        }

        const char *javaZero(Type t)
        {
            switch (t)
            {
            case Type::Int:
                return "0L";
            case Type::Float:
                return "0.0";
            case Type::Bool:
                return "false";
            default:
                return "0L";
            }
        }

        std::string operand(const IrProgram &ir, const Operand &o)
        {
            switch (o.kind)
            {
            case OperandKind::None:
                return "0L";
            case OperandKind::IntConst:
                return std::to_string(o.intValue) + "L";
            case OperandKind::FloatConst:
                return floatLiteral(o.floatValue);
            case OperandKind::Slot:
                return ir.slots[static_cast<std::size_t>(o.slot)].emit;
            }
            return "0L";
        }

        const char *javaBinary(Op op)
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
                return " / "; // long / long already truncates toward zero
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

    void emitJava(const IrProgram &ir, const std::string &sourcePath,
                  const std::string &className, std::ostream &out)
    {
        out << "// " << sourcePath << " থেকে সূত্র কম্পাইলার দিয়ে তৈরি।\n"
            << "// This file is generated. Edit the .sutro source instead.\n"
            << "public class " << className << " {\n"
            << "    public static void main(String[] args) {\n";

        // Every slot is declared up front and zero-initialised. Java requires a typed
        // declaration before use, and its definite-assignment rule would otherwise
        // reject a temporary that is only written inside one arm of a যদি. Hoisting
        // them all is also why the builder had to make the names unique: সূত্র allows
        // an inner scope to shadow an outer one, and two Java locals may not.
        for (const Slot &s : ir.slots)
        {
            out << "        " << javaType(s.type) << " " << s.emit
                << " = " << javaZero(s.type) << ";";
            if (!s.temporary)
                out << "   // " << s.name << "  " << s.line << ":" << s.column;
            out << "\n";
        }
        if (!ir.slots.empty())
            out << "\n";

        int indent = 2;
        const auto pad = [&out](int depth)
        {
            for (int i = 0; i < depth; ++i)
                out << "    ";
        };

        for (const Instr &in : ir.code)
        {
            if (in.op == Op::Else || in.op == Op::EndIf || in.op == Op::EndLoop)
                --indent;
            if (indent < 2)
                indent = 2;

            pad(indent);
            switch (in.op)
            {
            case Op::Move:
                out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                    << " = " << operand(ir, in.a) << ";\n";
                break;

            case Op::Neg:
                out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                    << " = -" << operand(ir, in.a) << ";\n";
                break;

            case Op::ToFloat:
                // The analyser decided this conversion. The emitter prints it and
                // never asks whether one is needed.
                out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                    << " = (double) " << operand(ir, in.a) << ";\n";
                break;

            case Op::Print:
                out << "System.out.println(" << operand(ir, in.a) << ");\n";
                break;

            case Op::If:
                out << "if (" << operand(ir, in.a) << ") {\n";
                ++indent;
                break;

            case Op::Else:
                out << "} else {\n";
                ++indent;
                break;

            case Op::EndIf:
            case Op::EndLoop:
                out << "}\n";
                break;

            case Op::Loop:
                out << "while (true) {\n";
                ++indent;
                break;

            case Op::ExitIfFalse:
                out << "if (!" << operand(ir, in.a) << ") break;\n";
                break;

            default:
                out << ir.slots[static_cast<std::size_t>(in.dst)].emit
                    << " = " << operand(ir, in.a) << javaBinary(in.op)
                    << operand(ir, in.b) << ";\n";
                break;
            }
        }

        out << "    }\n}\n";
    }

} // namespace sutro
