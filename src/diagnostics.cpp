#include "diagnostics.hpp"

#include <ostream>
#include <vector>

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

// A generated or machine-written line can be hundreds of kilobytes long. Echoing
// it whole would bury the one message it exists to illustrate, so anything past a
// screenful is shown as a window around the caret. Returns the windowed text and
// the column the caret lands on *inside that window*.
struct Window {
    std::string text;
    int column;
};

Window windowAround(const std::string& line, int column) {
    constexpr int kMaxVisible = 100;   // roughly a terminal width
    constexpr int kBefore     = 40;    // context kept to the left of the caret

    // Byte offset where each visible column starts. A combining mark belongs to
    // the column before it, so it opens no entry of its own.
    std::vector<std::size_t> colStart;
    for (std::size_t i = 0; i < line.size();) {
        const auto d = utf8::decode(line, i);
        if (!utf8::isCombiningMark(d.cp)) colStart.push_back(i);
        i += static_cast<std::size_t>(d.bytes);
    }

    const int total = static_cast<int>(colStart.size());
    if (total <= kMaxVisible) return {line, column};

    int first = column - kBefore;          // 1-based, inclusive
    if (first < 1) first = 1;
    int last = first + kMaxVisible;        // 1-based, exclusive
    if (last > total) {
        last = total;
        first = last - kMaxVisible;
        if (first < 1) first = 1;
    }

    const std::size_t from = colStart[static_cast<std::size_t>(first - 1)];
    const std::size_t to   = last >= total ? line.size()
                                           : colStart[static_cast<std::size_t>(last - 1)];

    const bool cutLeft  = first > 1;
    const bool cutRight = last < total;

    Window w;
    if (cutLeft) w.text = "…";
    w.text += line.substr(from, to - from);
    if (cutRight) w.text += "…";

    // The leading ellipsis occupies one visible column of its own.
    w.column = column - first + 1 + (cutLeft ? 1 : 0);
    return w;
}

} // namespace

void DiagnosticBag::printAll(const SourceFile& src, std::ostream& out) const {
    for (const Diagnostic& d : items_) {
        const bool isError = d.severity == Diagnostic::Severity::Error;
        const std::string kind = isError ? "ত্রুটি" : "সতর্কতা";

        out << src.path() << ":" << utf8::toBanglaDigits(d.line) << ":"
            << utf8::toBanglaDigits(d.column) << " " << kind
            << " [" << d.code << "]: " << d.message << "\n";

        const Window view = windowAround(src.line(d.line), d.column);
        if (!view.text.empty()) {
            const std::string gutter = utf8::toBanglaDigits(d.line);
            out << "  " << gutter << " | " << view.text << "\n";
            // The gutter is Bangla digits, three bytes each, so pad by digit count.
            out << "  " << std::string(gutter.size() / 3, ' ') << " | "
                << caretRow(view.text, view.column, d.length) << "\n";
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
