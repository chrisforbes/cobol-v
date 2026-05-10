#include <gtest/gtest.h>
#include "lexer.hpp"
#include "parser.hpp"

using namespace cobolv;

TEST(ParserTest, ParseMinimalShader) {
    const std::string source = R"(
IDENTIFICATION DIVISION.
PROGRAM-ID. BRIGHTNESS-FILTER.
DATA DIVISION.
PROCEDURE DIVISION.
    MAIN-LOGIC.
        GOBACK.
)";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();

    ASSERT_NE(program, nullptr);
    EXPECT_EQ(program->programId, "BRIGHTNESS-FILTER");
    
    // We expect 3 divisions: IDENTIFICATION, DATA, PROCEDURE
    ASSERT_EQ(program->divisions.size(), 3);
    
    // Check Identification Division (implicitly handled by programId for now, 
    // but should be in the list)
    EXPECT_EQ(program->divisions[0]->divisionType, TokenType::IDENTIFICATION);

    // Check Data Division
    EXPECT_EQ(program->divisions[1]->divisionType, TokenType::DATA);

    // Check Procedure Division
    auto* procDiv = program->divisions[2].get();
    ASSERT_EQ(procDiv->divisionType, TokenType::PROCEDURE);
    ASSERT_EQ(procDiv->sections.size(), 1);
    EXPECT_EQ(procDiv->sections[0]->name, "MAIN-LOGIC");
    ASSERT_EQ(procDiv->sections[0]->children.size(), 1);
    EXPECT_EQ(procDiv->sections[0]->children[0]->getType(), AstNodeType::STATEMENT);
}

TEST(ParserTest, ParseDataDeclarations) {
    const std::string source = R"(
IDENTIFICATION DIVISION.
PROGRAM-ID. DATA-TEST.
DATA DIVISION.
WORKING-STORAGE SECTION.
    01  MY-VAR PIC FLOAT.
    01  VEC-VAR PIC V(3).
PROCEDURE DIVISION.
    MAIN-LOGIC.
        GOBACK.
)";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();

    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->divisions.size(), 3);

    // Check Data Division
    auto* dataDiv = program->divisions[1].get();
    ASSERT_EQ(dataDiv->divisionType, TokenType::DATA);
    ASSERT_EQ(dataDiv->sections.size(), 1);
    EXPECT_EQ(dataDiv->sections[0]->name, "WORKING-STORAGE");
    
    auto& children = dataDiv->sections[0]->children;
    ASSERT_EQ(children.size(), 2);
    
    EXPECT_EQ(children[0]->getType(), AstNodeType::DATA_ITEM);
    EXPECT_EQ(children[1]->getType(), AstNodeType::DATA_ITEM);
}

TEST(ParserTest, ParseComplexDataTypes) {
    const std::string source = R"(
IDENTIFICATION DIVISION.
PROGRAM-ID. COMPLEX-DATA-TEST.
DATA DIVISION.
WORKING-STORAGE SECTION.
    01  INT-VEC   PIC IV(3).
    01  MAT-RECT  PIC M(4,2).
PROCEDURE DIVISION.
    MAIN-LOGIC.
        GOBACK.
)";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();

    ASSERT_NE(program, nullptr);
    auto* dataDiv = program->divisions[1].get();
    auto& children = dataDiv->sections[0]->children;
    ASSERT_EQ(children.size(), 2);
    
    auto* item1 = static_cast<DataItemNode*>(children[0].get());
    EXPECT_EQ(item1->name, "INT-VEC");
    EXPECT_EQ(item1->dataType.baseType, BaseType::INT);
    EXPECT_EQ(item1->dataType.vectorSize, 3);

    auto* item2 = static_cast<DataItemNode*>(children[1].get());
    EXPECT_EQ(item2->name, "MAT-RECT");
    EXPECT_EQ(item2->dataType.baseType, BaseType::FLOAT);
    EXPECT_EQ(item2->dataType.matrixRows, 4);
    EXPECT_EQ(item2->dataType.matrixCols, 2);
}
