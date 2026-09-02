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
    Error      // already reported; suppresses further complaints about this node
};

inline const char* typeName(Type t) {
    switch (t) {
        case Type::Int:     return "পূর্ণ";
        case Type::Float:   return "দশমিক";
        case Type::Error:   return "ত্রুটি";
        case Type::Unknown: return "?";
    }
    return "?";
}

} // namespace sutro
