// One error channel for the whole compiler.
//
// No phase throws and no phase prints. Every phase reports into the same bag, so
// a lexical error and a type error reach the student in exactly the same shape,
// and a broken program still produces a full list instead of only the first
// problem. Each diagnostic carries a stable code (L01, P02, T03 ...) so tests can
// assert on the code and stay valid when we improve the wording.
#pragma once

#include <string>
#include <vector>

#include "source.hpp"

namespace sutro {

struct Diagnostic {
    enum class Severity { Error, Warning };

    Severity severity = Severity::Error;
    std::string code;      // "L01" lexer, "P01" parser, "T01" types
    std::string message;   // Bangla, shown to the student
    std::string hint;      // Bangla, optional: what to do about it
    int line = 0;          // 1-based
    int column = 0;        // 1-based, counted in visible characters
    int length = 1;        // how many characters the caret should underline
};

class DiagnosticBag {
public:
    void error(const std::string& code, const std::string& message,
               const std::string& hint, int line, int column, int length = 1);
    void warning(const std::string& code, const std::string& message,
                 const std::string& hint, int line, int column, int length = 1);

    bool hasErrors() const { return errorCount_ > 0; }
    int errorCount() const { return errorCount_; }
    bool empty() const { return items_.empty(); }
    const std::vector<Diagnostic>& all() const { return items_; }

    // Renders every diagnostic with the offending line and a caret under it.
    void printAll(const SourceFile& src, std::ostream& out) const;

private:
    std::vector<Diagnostic> items_;
    int errorCount_ = 0;
};

} // namespace sutro
