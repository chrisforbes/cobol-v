#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.hpp"
#include "parser.hpp"
#include "semantics.hpp"
#include "emitter.hpp"
#include "ast_dumper.hpp"
#include "symbol_dumper.hpp"

int main(int argc, char* argv[]) {
    try {
        std::string filename;
        std::string outFilename = "output.spv";
        bool dumpAst = false;
        bool dumpSymbols = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--dump-ast") dumpAst = true;
            else if (arg == "--dump-symbols") dumpSymbols = true;
            else if (filename.empty()) filename = arg;
            else outFilename = arg;
        }

        if (filename.empty()) {
            std::cerr << "Usage: " << argv[0] << " [--dump-ast] [--dump-symbols] <source.cob> [output.spv]" << std::endl;
            return 1;
        }

        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << filename << std::endl;
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        cobolv::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        cobolv::Parser parser(tokens);
        auto ast = parser.parse();

        if (dumpAst) {
            cobolv::AstDumper dumper;
            dumper.dump(*ast);
            return 0;
        }

        cobolv::SemanticAnalyzer analyzer;
        analyzer.analyze(*ast);
 
        if (dumpSymbols) {
            cobolv::SymbolDumper dumper;
            dumper.dump(analyzer.getSymbolTable());
            return 0;
        }

        cobolv::Emitter emitter;
        auto binary = emitter.emit(*ast, analyzer.getRequiredCapabilities());

        std::ofstream outFile(outFilename, std::ios::binary);
        if (outFile.is_open()) {
            outFile.write(reinterpret_cast<const char*>(binary.data()), binary.size() * sizeof(uint32_t));
            outFile.close();
            std::cout << "Successfully compiled " << filename << " to " << outFilename << std::endl;
        } else {
            std::cerr << "Could not open output file: " << outFilename << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred during compilation." << std::endl;
        return 1;
    }

    return 0;
}
