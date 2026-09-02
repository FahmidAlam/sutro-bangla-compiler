#include "source.hpp"

#include <fstream>
#include <sstream>

namespace sutro {

bool SourceFile::load(const std::string& path, SourceFile& out, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "ফাইল খোলা যায়নি: " + path;
        return false;
    }
    std::ostringstream buf;
    buf << in.rdbuf();

    out.path_ = path;
    out.text_ = buf.str();

    // Notepad and friends like to prepend a UTF-8 BOM. Drop it, or the lexer sees
    // a stray character before the first keyword.
    if (out.text_.size() >= 3 &&
        static_cast<unsigned char>(out.text_[0]) == 0xEF &&
        static_cast<unsigned char>(out.text_[1]) == 0xBB &&
        static_cast<unsigned char>(out.text_[2]) == 0xBF) {
        out.text_.erase(0, 3);
    }

    out.indexLines();
    return true;
}

void SourceFile::indexLines() {
    lineStarts_.clear();
    lineStarts_.push_back(0);
    for (std::size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\n') lineStarts_.push_back(i + 1);
    }
}

std::string SourceFile::line(int lineNo) const {
    if (lineNo < 1 || lineNo > lineCount()) return {};
    const std::size_t begin = lineStarts_[static_cast<std::size_t>(lineNo - 1)];
    std::size_t end = (lineNo < lineCount()) ? lineStarts_[static_cast<std::size_t>(lineNo)]
                                             : text_.size();
    while (end > begin && (text_[end - 1] == '\n' || text_[end - 1] == '\r')) --end;
    return text_.substr(begin, end - begin);
}

} // namespace sutro
