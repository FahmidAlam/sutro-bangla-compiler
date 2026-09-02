// Driver: reads a .sutro file, runs the phases the user asked for, prints the result.
//
//   sutro <file.sutro> [--emit=tokens|ast|sym]
//
// --emit stops the pipeline after a phase and dumps what that phase produced. It
// is how we debug, and how any team member can show any phase during a review.
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ast.hpp"
#include "diagnostics.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include "source.hpp"
#include "token.hpp"
#include "utf8.hpp"

namespace {

void printUsage() {
    std::cout <<
        "সূত্র — Shutro compiler\n"
        "\n"
        "  sutro <file.sutro> [--emit=tokens|ast]\n"
        "\n"
        "  --emit=tokens   dump the token stream and stop\n"
        "  --emit=ast      dump the syntax tree and stop\n"
        "  -h, --help      show this message\n";
}

void dumpTokens(const std::vector<sutro::Token>& tokens) {
    for (const sutro::Token& t : tokens) {
        const std::string pos = std::to_string(t.line) + ":" + std::to_string(t.column);
        std::string value;
        if (t.kind == sutro::TokenKind::IntLiteral)   value = std::to_string(t.intValue);
        if (t.kind == sutro::TokenKind::FloatLiteral) value = std::to_string(t.floatValue);

        std::cout << "  " << sutro::utf8::padTo(pos, 8)
                  << sutro::utf8::padTo(sutro::tokenKindName(t.kind), 8)
                  << sutro::utf8::padTo(value, 10)
                  << t.lexeme << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    // Without this the console prints Bangla as mojibake, and every error message
    // we worked hard on becomes unreadable.
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::string path;
    std::string emit;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        }
        if (arg.rfind("--emit=", 0) == 0) {
            emit = arg.substr(7);
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "unknown option: " << arg << "\n";
            printUsage();
            return 2;
        }
        path = arg;
    }

    if (path.empty()) {
        printUsage();
        return 2;
    }

    sutro::SourceFile source;
    std::string ioError;
    if (!sutro::SourceFile::load(path, source, ioError)) {
        std::cerr << ioError << "\n";
        return 2;
    }

    sutro::DiagnosticBag diagnostics;

    sutro::Lexer lexer(source, diagnostics);
    const std::vector<sutro::Token> tokens = lexer.tokenize();

    if (emit == "tokens") {
        dumpTokens(tokens);
        diagnostics.printAll(source, std::cerr);
        return diagnostics.hasErrors() ? 1 : 0;
    }

    sutro::Parser parser(tokens, diagnostics);
    sutro::Program program = parser.parse();

    // Semantic analysis is skipped when the program did not parse cleanly: the
    // tree is missing whatever the parser threw away, so every name it dropped
    // would come back as a bogus "undeclared" on top of the real syntax error.
    sutro::SemanticAnalyzer sema(diagnostics);
    if (!diagnostics.hasErrors()) sema.analyze(program);

    if (emit == "ast") {
        sutro::printAst(program, std::cout);
        diagnostics.printAll(source, std::cerr);
        return diagnostics.hasErrors() ? 1 : 0;
    }

    if (emit == "sym") {
        sema.symbols().print(std::cout);
        diagnostics.printAll(source, std::cerr);
        return diagnostics.hasErrors() ? 1 : 0;
    }

    if (!emit.empty()) {
        std::cerr << "not implemented yet: --emit=" << emit << "\n";
        return 2;
    }

    // Later phases arrive here: IR construction, then code generation.
    diagnostics.printAll(source, std::cerr);
    if (diagnostics.hasErrors()) return 1;

    std::cout << "ঠিক আছে — " << program.statements.size()
              << " টি বিবৃতি পার্স করা হয়েছে।\n";
    return 0;
}
