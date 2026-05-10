#pragma once

#include <string>
#include <vector>
#include <map>

namespace cobolv {

enum class TokenType {
    // Literals & Identifiers
    IDENTIFIER, NUMBER, STRING,
    
    // Divisions & Sections
    IDENTIFICATION, ENVIRONMENT, DATA, PROCEDURE, DIVISION,
    CONFIGURATION, INPUT_OUTPUT, FILE_CONTROL, FILE_SECTION,
    LINKAGE, WORKING_STORAGE, SECTION,
    
    // Keywords
    PROGRAM_ID, SPECIAL_NAMES, LOCAL_SIZE, IS, ASSIGN, TO,
    ORGANIZATION, SELECT, FD, BASED, PIC, USAGE, POINTER,
    BUILT_IN, SET, ADDRESS, OF, ACCEPT, FROM, COMPUTE,
    READ, WRITE, AT, WITH, INTO, CALL, USING, CONSULT, COHORT, FOR, 
    FETCH, GATHER, COMPONENT,
    SUM, PRODUCT, MIN, MAX, XOR, GIVING,
    ANY, ALL, ELECT, BALLOT, BROADCAST, BROADCAST_FIRST,
    REDUCE, INCLUSIVE_SCAN, EXCLUSIVE_SCAN,
    SYNC, WORKGROUP, LOCAL_STORAGE,
    INQUIRE, SIZE, LOD, LEVELS, GRADIENTS, COMPARING, PROJECTION,
    AND, OR, ATOMIC_ADD, ATOMIC_SUB, ATOMIC_AND, ATOMIC_OR, ATOMIC_XOR,
    ATOMIC_MIN, ATOMIC_MAX, ATOMIC_EXCHANGE, ATOMIC_COMPARE_EXCHANGE,
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, BIT_LSHIFT, BIT_RSHIFT,
    GOBACK, DISCARD, VALUE,
    LESS, THAN, EQUAL, GREATER,
    GREATER_EQUAL, LESS_EQUAL, NOT_EQUAL,
    IF, ELSE, END_IF, PERFORM, VARYING, BY, UNTIL, TIMES, END_PERFORM, MOVE, NOT,
    OCCURS,
    FLOAT, I, V,
    U, B, M, IV, UV, BV,
    
    // Graphics Stages
    OBJECT_COMPUTER, VULKAN_VERTEX_SHADER, VULKAN_FRAGMENT_SHADER, VULKAN_COMPUTE_SHADER,
    INPUT, OUTPUT, LOCATION,
    
    // Swizzling / Components
    X, Y, Z, W, RED, GREEN, BLUE, ALPHA,
    IMAGE_1D, IMAGE_2D, IMAGE_3D, IMAGE_1D_ARRAY, IMAGE_2D_ARRAY, IMAGE_CUBE, IMAGE_CUBE_ARRAY,
    TEXTURE_1D, TEXTURE_2D, TEXTURE_3D, TEXTURE_1D_ARRAY, TEXTURE_2D_ARRAY, TEXTURE_CUBE, TEXTURE_CUBE_ARRAY,
    STORAGE_1D, STORAGE_2D, STORAGE_3D, STORAGE_1D_ARRAY, STORAGE_2D_ARRAY, STORAGE_CUBE, STORAGE_CUBE_ARRAY,
    SAMPLER, ACCESS_PUSH, ACCESS_UNIFORM, ACCESS_STORAGE, BUFFER,
    LAYOUT, STD140, STD430, SCALAR,
    ACCESS, MODE, READ_ONLY, WRITE_ONLY, READ_WRITE, FORMAT, COHERENT, VOLATILE,
    INTERPOLATION, FLAT, NOPERSPECTIVE, SMOOTH,

    // Symbols
    PERIOD, LPAREN, RPAREN, COMMA, EQUALS, 
    PLUS, MINUS, STAR, SLASH,
    GREATER_SYM, LESS_SYM,
    GREATER_EQUAL_SYM, LESS_EQUAL_SYM, NOT_EQUAL_SYM,
    
    END_OF_FILE, UNKNOWN
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

class Lexer {
public:
    Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string source;
    size_t start = 0;
    size_t current = 0;
    int line = 1;
    int column = 1;

    static const std::map<std::string, TokenType> keywords;

    bool isAtEnd() const;
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);
    
    void skipWhitespace();
    Token makeToken(TokenType type);
    Token identifier();
    Token number();
    Token string();
};

} // namespace cobolv
