// The source file, held once in memory, plus an index of where each line starts.
// Every diagnostic needs to reprint the offending line, so that index is the only
// state required to turn a (line, column) back into text.
#pragma once

#include <string>
#include <vector>

namespace sutro {

class SourceFile {
public:
    // Returns false and fills `error` if the file cannot be opened.
    static bool load(const std::string& path, SourceFile& out, std::string& error);

    const std::string& path() const { return path_; }
    const std::string& text() const { return text_; }

    // 1-based, newline stripped. Out-of-range line numbers give an empty string.
    std::string line(int lineNo) const;
    int lineCount() const { return static_cast<int>(lineStarts_.size()); }

private:
    void indexLines();

    std::string path_;
    std::string text_;
    std::vector<std::size_t> lineStarts_;
};

} // namespace sutro
