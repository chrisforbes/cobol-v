#include "lexer.hpp"
#include <cctype>

namespace cobolv {

const std::map<std::string, TokenType> Lexer::keywords = {
    {"IDENTIFICATION", TokenType::IDENTIFICATION},
    {"ENVIRONMENT", TokenType::ENVIRONMENT},
    {"DATA", TokenType::DATA},
    {"PROCEDURE", TokenType::PROCEDURE},
    {"DIVISION", TokenType::DIVISION},
    {"SECTION", TokenType::SECTION},
    {"CONFIGURATION", TokenType::CONFIGURATION},
    {"INPUT-OUTPUT", TokenType::INPUT_OUTPUT},
    {"FILE-CONTROL", TokenType::FILE_CONTROL},
    {"FILE", TokenType::FILE_SECTION},
    {"LINKAGE", TokenType::LINKAGE},
    {"WORKING-STORAGE", TokenType::WORKING_STORAGE},
    {"PROGRAM-ID", TokenType::PROGRAM_ID},
    {"SPECIAL-NAMES", TokenType::SPECIAL_NAMES},
    {"LOCAL-SIZE", TokenType::LOCAL_SIZE},
    {"IS", TokenType::IS},
    {"ASSIGN", TokenType::ASSIGN},
    {"TO", TokenType::TO},
    {"ORGANIZATION", TokenType::ORGANIZATION},
    {"SELECT", TokenType::SELECT},
    {"FD", TokenType::FD},
    {"BASED", TokenType::BASED},
    {"PIC", TokenType::PIC},
    {"USAGE", TokenType::USAGE},
    {"POINTER", TokenType::POINTER},
    {"BUILT-IN", TokenType::BUILT_IN},
    {"SET", TokenType::SET},
    {"ADDRESS", TokenType::ADDRESS},
    {"OF", TokenType::OF},
    {"ACCEPT", TokenType::ACCEPT},
    {"FROM", TokenType::FROM},
    {"COMPUTE", TokenType::COMPUTE},
    {"READ", TokenType::READ},
    {"WRITE", TokenType::WRITE},
    {"AT", TokenType::AT},
    {"WITH", TokenType::WITH},
    {"INTO", TokenType::INTO},
    {"CALL", TokenType::CALL},
    {"USING", TokenType::USING},
    {"CONSULT", TokenType::CONSULT},
    {"COHORT", TokenType::COHORT},
    {"FOR", TokenType::FOR},
    {"FETCH", TokenType::FETCH},
    {"GATHER", TokenType::GATHER},
    {"COMPONENT", TokenType::COMPONENT},
    {"SUM", TokenType::SUM},
    {"PRODUCT", TokenType::PRODUCT},
    {"MIN", TokenType::MIN},
    {"MAX", TokenType::MAX},
    {"XOR", TokenType::XOR},
    {"ANY", TokenType::ANY},
    {"ALL", TokenType::ALL},
    {"ELECT", TokenType::ELECT},
    {"BALLOT", TokenType::BALLOT},
    {"BROADCAST", TokenType::BROADCAST},
    {"BROADCAST-FIRST", TokenType::BROADCAST_FIRST},
    {"REDUCE", TokenType::REDUCE},
    {"INCLUSIVE-SCAN", TokenType::INCLUSIVE_SCAN},
    {"EXCLUSIVE-SCAN", TokenType::EXCLUSIVE_SCAN},
    {"SYNC", TokenType::SYNC},
    {"WORKGROUP", TokenType::WORKGROUP},
    {"INQUIRE", TokenType::INQUIRE},
    {"FOR", TokenType::FOR},
    {"SIZE", TokenType::SIZE},
    {"LOD", TokenType::LOD},
    {"GRADIENTS", TokenType::GRADIENTS},
    {"COMPARING", TokenType::COMPARING},
    {"PROJECTION", TokenType::PROJECTION},
    {"LEVELS", TokenType::LEVELS},
    {"GIVING", TokenType::GIVING},
    {"AND", TokenType::AND},
    {"OR", TokenType::OR},
    {"LOCAL-STORAGE", TokenType::LOCAL_STORAGE},
    {"ATOMIC-ADD", TokenType::ATOMIC_ADD},
    {"ATOMIC-SUB", TokenType::ATOMIC_SUB},
    {"ATOMIC-AND", TokenType::ATOMIC_AND},
    {"ATOMIC-OR", TokenType::ATOMIC_OR},
    {"ATOMIC-XOR", TokenType::ATOMIC_XOR},
    {"ATOMIC-MIN", TokenType::ATOMIC_MIN},
    {"ATOMIC-MAX", TokenType::ATOMIC_MAX},
    {"ATOMIC-EXCHANGE", TokenType::ATOMIC_EXCHANGE},
    {"ATOMIC-COMPARE-EXCHANGE", TokenType::ATOMIC_COMPARE_EXCHANGE},
    {"BIT-AND", TokenType::BIT_AND},
    {"BIT-OR", TokenType::BIT_OR},
    {"BIT-XOR", TokenType::BIT_XOR},
    {"BIT-NOT", TokenType::BIT_NOT},
    {"BIT-LSHIFT", TokenType::BIT_LSHIFT},
    {"BIT-RSHIFT", TokenType::BIT_RSHIFT},
    {"GOBACK", TokenType::GOBACK},
    {"DISCARD", TokenType::DISCARD},
    {"VALUE", TokenType::VALUE},
    {"LESS", TokenType::LESS},
    {"THAN", TokenType::THAN},
    {"EQUAL", TokenType::EQUAL},
    {"GREATER", TokenType::GREATER},
    {"IF", TokenType::IF},
    {"ELSE", TokenType::ELSE},
    {"END-IF", TokenType::END_IF},
    {"PERFORM", TokenType::PERFORM},
    {"VARYING", TokenType::VARYING},
    {"BY", TokenType::BY},
    {"UNTIL", TokenType::UNTIL},
    {"TIMES", TokenType::TIMES},
    {"END-PERFORM", TokenType::END_PERFORM},
    {"MOVE", TokenType::MOVE},
    {"FLOAT-TO-INT", TokenType::FLOAT_TO_INT},
    {"FLOAT-TO-UINT", TokenType::FLOAT_TO_UINT},
    {"INT-TO-FLOAT", TokenType::INT_TO_FLOAT},
    {"UINT-TO-FLOAT", TokenType::UINT_TO_FLOAT},
    {"INT-TO-UINT", TokenType::INT_TO_UINT},
    {"UINT-TO-INT", TokenType::UINT_TO_INT},
    {"NOT", TokenType::NOT},
    {"OCCURS", TokenType::OCCURS},
    {"FLOAT", TokenType::FLOAT},
    {"I", TokenType::I},
    {"V", TokenType::V},
    {"U", TokenType::U},
    {"B", TokenType::B},
    {"M", TokenType::M},
    {"IV", TokenType::IV},
    {"UV", TokenType::UV},
    {"BV", TokenType::BV},
    {"X", TokenType::X},
    {"Y", TokenType::Y},
    {"Z", TokenType::Z},
    {"W", TokenType::W},
    {"RED", TokenType::RED},
    {"GREEN", TokenType::GREEN},
    {"BLUE", TokenType::BLUE},
    {"ALPHA", TokenType::ALPHA},
    {"IMAGE-1D", TokenType::IMAGE_1D},
    {"IMAGE-2D", TokenType::IMAGE_2D},
    {"IMAGE-3D", TokenType::IMAGE_3D},
    {"IMAGE-1D-ARRAY", TokenType::IMAGE_1D_ARRAY},
    {"IMAGE-2D-ARRAY", TokenType::IMAGE_2D_ARRAY},
    {"IMAGE-CUBE", TokenType::IMAGE_CUBE},
    {"IMAGE-CUBE-ARRAY", TokenType::IMAGE_CUBE_ARRAY},
    {"TEXTURE-1D", TokenType::TEXTURE_1D},
    {"TEXTURE-2D", TokenType::TEXTURE_2D},
    {"TEXTURE-3D", TokenType::TEXTURE_3D},
    {"TEXTURE-1D-ARRAY", TokenType::TEXTURE_1D_ARRAY},
    {"TEXTURE-2D-ARRAY", TokenType::TEXTURE_2D_ARRAY},
    {"TEXTURE-CUBE", TokenType::TEXTURE_CUBE},
    {"TEXTURE-CUBE-ARRAY", TokenType::TEXTURE_CUBE_ARRAY},
    {"STORAGE-1D", TokenType::STORAGE_1D},
    {"STORAGE-2D", TokenType::STORAGE_2D},
    {"STORAGE-3D", TokenType::STORAGE_3D},
    {"STORAGE-1D-ARRAY", TokenType::STORAGE_1D_ARRAY},
    {"STORAGE-2D-ARRAY", TokenType::STORAGE_2D_ARRAY},
    {"STORAGE-CUBE", TokenType::STORAGE_CUBE},
    {"STORAGE-CUBE-ARRAY", TokenType::STORAGE_CUBE_ARRAY},
    {"SAMPLER", TokenType::SAMPLER},
    {"ACCESS-PUSH", TokenType::ACCESS_PUSH},
    {"ACCESS-UNIFORM", TokenType::ACCESS_UNIFORM},
    {"ACCESS-STORAGE", TokenType::ACCESS_STORAGE},
    {"BUFFER", TokenType::BUFFER},
    {"LAYOUT", TokenType::LAYOUT},
    {"STD140", TokenType::STD140},
    {"STD430", TokenType::STD430},
    {"SCALAR", TokenType::SCALAR},
    {"ACCESS", TokenType::ACCESS},
    {"MODE", TokenType::MODE},
    {"READ-ONLY", TokenType::READ_ONLY},
    {"WRITE-ONLY", TokenType::WRITE_ONLY},
    {"READ-WRITE", TokenType::READ_WRITE},
    {"FORMAT", TokenType::FORMAT},
    {"COHERENT", TokenType::COHERENT},
    {"VOLATILE", TokenType::VOLATILE},
    {"OBJECT-COMPUTER", TokenType::OBJECT_COMPUTER},
    {"VULKAN-VERTEX-SHADER", TokenType::VULKAN_VERTEX_SHADER},
    {"VULKAN-FRAGMENT-SHADER", TokenType::VULKAN_FRAGMENT_SHADER},
    {"VULKAN-COMPUTE-SHADER", TokenType::VULKAN_COMPUTE_SHADER},
    {"INPUT", TokenType::INPUT},
    {"OUTPUT", TokenType::OUTPUT},
    {"LOCATION", TokenType::LOCATION},
    {"INTERPOLATION", TokenType::INTERPOLATION},
    {"FLAT", TokenType::FLAT},
    {"NOPERSPECTIVE", TokenType::NOPERSPECTIVE},
    {"SMOOTH", TokenType::SMOOTH}
};

Lexer::Lexer(const std::string& source) : source(source) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) break;

        start = current;
        char c = advance();
        int col = column - 1;

        if (std::isalpha(c)) {
            tokens.push_back(identifier());
        } else if (std::isdigit(c)) {
            tokens.push_back(number());
        } else {
            switch (c) {
                case '.': tokens.push_back(makeToken(TokenType::PERIOD)); break;
                case '(': tokens.push_back(makeToken(TokenType::LPAREN)); break;
                case ')': tokens.push_back(makeToken(TokenType::RPAREN)); break;
                case ',': tokens.push_back(makeToken(TokenType::COMMA)); break;
                case '>':
                    if (match('=')) tokens.push_back({TokenType::GREATER_EQUAL_SYM, ">=", line, col});
                    else tokens.push_back({TokenType::GREATER_SYM, ">", line, col});
                    break;
                case '<':
                    if (match('=')) tokens.push_back({TokenType::LESS_EQUAL_SYM, "<=", line, col});
                    else if (match('>')) tokens.push_back({TokenType::NOT_EQUAL_SYM, "<>", line, col});
                    else tokens.push_back({TokenType::LESS_SYM, "<", line, col});
                    break;
                case '=': tokens.push_back({TokenType::EQUALS, "=", line, col}); break;
                case '+': tokens.push_back({TokenType::PLUS, "+", line, col}); break;
                case '-': tokens.push_back(makeToken(TokenType::MINUS)); break;
                case '*': tokens.push_back(makeToken(TokenType::STAR)); break;
                case '/': tokens.push_back(makeToken(TokenType::SLASH)); break;
                case '"': tokens.push_back(string()); break;
                default:
                    tokens.push_back(makeToken(TokenType::UNKNOWN));
                    break;
            }
        }
    }
    tokens.push_back(makeToken(TokenType::END_OF_FILE));
    return tokens;
}

bool Lexer::isAtEnd() const { return current >= source.length(); }

char Lexer::advance() {
    column++;
    return source[current++];
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;
    current++;
    column++;
    return true;
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length()) return '\0';
    return source[current + 1];
}

void Lexer::skipWhitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                line++;
                column = 0;
                advance();
                break;
            case '*':
                if (column == 1) {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else if (peekNext() == '>') {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

Token Lexer::makeToken(TokenType type) {
    return {type, source.substr(start, current - start), line, column - (int)(current - start)};
}

Token Lexer::identifier() {
    while (std::isalnum(peek()) || peek() == '-') advance();
    
    std::string text = source.substr(start, current - start);
    TokenType type = TokenType::IDENTIFIER;
    
    if (keywords.count(text)) {
        type = keywords.at(text);
    }
    
    return makeToken(type);
}

Token Lexer::number() {
    while (std::isdigit(peek())) advance();
    if (peek() == '.' && std::isdigit(peekNext())) {
        advance();
        while (std::isdigit(peek())) advance();
    }
    return makeToken(TokenType::NUMBER);
}

Token Lexer::string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') { line++; column = 1; }
        advance();
    }
    if (!isAtEnd()) advance();
    return makeToken(TokenType::STRING);
}

} // namespace cobolv
