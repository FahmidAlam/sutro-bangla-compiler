// The type lattice. সূত্র has exactly two types a student can write, plus two
// internal ones the compiler needs: Unknown before the analyser has run, and
// Error after a type mistake, so one bad expression does not cascade.
#pragma once

#include <string>

namespace sutro {

enum class Type {
    Unknown,   // not yet decided — every node starts here
    Int,       // পূর্ণ
    Float,     // দশমিক
    Bool,      // internal only — a student cannot write it and no variable can hold
               // it. It is the type of a comparison, which the grammar allows only
               // at the top of a যদি / যতক্ষণ condition. Naming it costs one
               // enumerator and buys the Java backend a real boolean for if(...),
               // instead of an emitter inventing '!= 0' on its own.
    Error      // already reported; suppresses further complaints about this node
};

inline const char* typeName(Type t) {
    switch (t) {
        case Type::Int:     return "পূর্ণ";
        case Type::Float:   return "দশমিক";
        case Type::Bool:    return "শর্ত";
        case Type::Error:   return "ত্রুটি";
        case Type::Unknown: return "?";
    }
    return "?";
}

} // namespace sutro
