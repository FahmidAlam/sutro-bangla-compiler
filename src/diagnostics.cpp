#include "diagnostics.hpp"

#include <ostream>

#include "utf8.hpp"

namespace sutro {

void DiagnosticBag::error(const std::string& code, const std::string& message,
                          const std::string& hint, int line, int column, int length) {
    items_.push_back({Diagnostic::Severity::Error, code, message, hint, line, column, length});
    ++errorCount_;
}

void DiagnosticBag::warning(const std::string& code, const std::string& message,
                            const std::string& hint, int line, int column, int length) {
    items_.push_back({Diagnostic::Severity::Warning, code, message, hint, line, column, length});
}

namespace {

// Builds the caret row. Bangla matras hang off the previous letter and take no
// column of their own, so they are skipped; a tab is copied through, otherwise
// the caret drifts away from the character it is pointing at.
std::string caretRow(const std::string& lineText, int column, int length) {
    std::string pad;
    int seen = 0;
    std::size_t i = 0;
    while (i < lineText.size() && seen < column - 1) {
        const auto d = utf8::decode(lineText, i);
        if (!utf8::isCombiningMark(d.cp)) {
            pad += (d.cp == '\t') ? '\t' : ' ';
            ++seen;
        }
        i += static_cast<std::size_t>(d.bytes);
    }
    return pad + std::string(length < 1 ? 1 : static_cast<std::size_t>(length), '^');
}

} // namespace

void DiagnosticBag::printAll(const SourceFile& src, std::ostream& out) const {
    for (const Diagnostic& d : items_) {
        const bool isError = d.severity == Diagnostic::Severity::Error;
        const std::string kind = isError ? "ত্রুটি" : "সতর্কতা";

        out << src.path() << ":" << utf8::toBanglaDigits(d.line) << ":"
            << utf8::toBanglaDigits(d.column) << " " << kind
            << " [" << d.code << "]: " << d.message << "\n";

        const std::string text = src.line(d.line);
        if (!text.empty()) {
            const std::string gutter = utf8::toBanglaDigits(d.line);
            out << "  " << gutter << " | " << text << "\n";
            // The gutter is Bangla digits, three bytes each, so pad by digit count.
            out << "  " << std::string(gutter.size() / 3, ' ') << " | "
                << caretRow(text, d.column, d.length) << "\n";
        }
        if (!d.hint.empty()) {
            out << "    = ইঙ্গিত: " << d.hint << "\n";
        }
        out << "\n";
    }

    if (errorCount_ > 0) {
        out << utf8::toBanglaDigits(errorCount_) << " টি ত্রুটি পাওয়া গেছে।\n";
    }
}

} // namespace sutro
