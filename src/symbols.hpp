// The symbol table: what names exist, what type each one has, and where it was
// written.
//
// Scopes are a stack of maps from name to an index into `symbols_`, and lookup
// walks that stack innermost-first. The symbols themselves are never removed —
// closing a scope pops the map, not the entries — because --emit=sym has to be
// able to print every name the program declared, including ones whose scope has
// already closed. That is also why `symbols_` is a deque: pointers handed out by
// lookup() stay valid when a later declaration grows the container.
#pragma once

#include <cstddef>
#include <deque>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

#include "type.hpp"

namespace sutro {

struct Symbol {
    std::string name;
    Type type = Type::Unknown;
    int line = 0;          // where it was declared, 1-based
    int column = 0;
    int depth = 0;         // 0 is file scope, one deeper per { }
    int uses = 0;          // reads only; drives the "declared but never used" warning
};

class SymbolTable {
public:
    SymbolTable();         // opens file scope, which is never popped

    void pushScope();
    void popScope();
    int depth() const { return static_cast<int>(scopes_.size()) - 1; }

    // Declares in the innermost scope. Returns nullptr if the name is already
    // declared *in that same scope*; the caller reports the clash against
    // lookupCurrentScope(), which still holds the earlier declaration.
    Symbol* declare(const std::string& name, Type type, int line, int column);

    Symbol* lookup(const std::string& name);              // innermost first
    Symbol* lookupCurrentScope(const std::string& name);  // this scope only

    // Declaration order, closed scopes included.
    const std::deque<Symbol>& all() const { return symbols_; }

    // --emit=sym
    void print(std::ostream& out) const;

private:
    std::deque<Symbol> symbols_;
    std::vector<std::unordered_map<std::string, std::size_t>> scopes_;
};

} // namespace sutro
