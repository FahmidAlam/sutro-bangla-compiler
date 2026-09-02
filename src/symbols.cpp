#include "symbols.hpp"

#include <ostream>

#include "utf8.hpp"

namespace sutro {

SymbolTable::SymbolTable() {
    scopes_.emplace_back();   // file scope
}

void SymbolTable::pushScope() {
    scopes_.emplace_back();
}

void SymbolTable::popScope() {
    // File scope stays: popping it would leave lookup() with nothing to search.
    if (scopes_.size() > 1) scopes_.pop_back();
}

Symbol* SymbolTable::declare(const std::string& name, Type type, int line, int column) {
    auto& scope = scopes_.back();
    if (scope.count(name) != 0) return nullptr;

    symbols_.push_back(Symbol{name, type, line, column, depth(), 0});
    scope.emplace(name, symbols_.size() - 1);
    return &symbols_.back();
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        const auto found = it->find(name);
        if (found != it->end()) return &symbols_[found->second];
    }
    return nullptr;
}

Symbol* SymbolTable::lookupCurrentScope(const std::string& name) {
    const auto& scope = scopes_.back();
    const auto found = scope.find(name);
    return found == scope.end() ? nullptr : &symbols_[found->second];
}

void SymbolTable::print(std::ostream& out) const {
    // Headers stay ASCII and positions stay in ASCII digits, like --emit=tokens:
    // the dumps are a developer tool, and Bangla digits are reserved for the
    // diagnostics a student actually reads.
    out << "  " << utf8::padTo("scope", 8)
        << utf8::padTo("name", 14)
        << utf8::padTo("type", 10)
        << utf8::padTo("declared", 12)
        << "uses\n";

    for (const Symbol& s : symbols_) {
        const std::string at = std::to_string(s.line) + ":" + std::to_string(s.column);
        out << "  " << utf8::padTo(std::to_string(s.depth), 8)
            << utf8::padTo(s.name, 14)
            << utf8::padTo(typeName(s.type), 10)
            << utf8::padTo(at, 12)
            << s.uses << "\n";
    }

    if (symbols_.empty()) out << "  (কোনো নাম ঘোষণা করা হয়নি)\n";
}

} // namespace sutro
