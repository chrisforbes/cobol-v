#pragma once

#include "lexer.hpp"
#include "ast.hpp"
#include <vector>
#include <memory>

namespace cobolv {

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<ProgramNode> parse();

private:
    std::vector<Token> tokens;
    size_t current = 0;

    bool isAtEnd() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string& message);
    bool checkIdentifier() const;
    const Token& consumeIdentifier(const std::string& message);

    std::unique_ptr<DivisionNode> parseIdentificationDivision(ProgramNode& program);
    std::unique_ptr<DivisionNode> parseEnvironmentDivision(ProgramNode& program);
    std::unique_ptr<SectionNode> parseConfigurationSection(ProgramNode& program);
    std::unique_ptr<SectionNode> parseSpecialNames();
    std::unique_ptr<DivisionNode> parseInputOutputSection();
    std::unique_ptr<SectionNode> parseFileControl();
    std::unique_ptr<AstNode> parseSelect();
    std::unique_ptr<DivisionNode> parseDataDivision();
    std::unique_ptr<SectionNode> parseFileSection();
    std::unique_ptr<FileDescriptionNode> parseFileDescription();
    std::unique_ptr<SectionNode> parseWorkingStorageSection();
    std::unique_ptr<SectionNode> parseLocalStorageSection();
    std::unique_ptr<SectionNode> parseLinkageSection();
    std::unique_ptr<SectionNode> parseInputSection();
    std::unique_ptr<SectionNode> parseOutputSection();
    std::unique_ptr<DataItemNode> parseDataItem();
    std::unique_ptr<DivisionNode> parseProcedureDivision();
    std::unique_ptr<SectionNode> parseSection();
    std::unique_ptr<StatementNode> parseStatement();
    std::unique_ptr<StatementNode> parseMoveStatement();
    std::unique_ptr<StatementNode> parseComputeStatement();
    std::unique_ptr<StatementNode> parseIfStatement();
    std::unique_ptr<StatementNode> parsePerformStatement();
    std::unique_ptr<StatementNode> parseReadStatement();
    std::unique_ptr<StatementNode> parseWriteStatement();
    std::unique_ptr<StatementNode> parseCallStatement();
    std::unique_ptr<StatementNode> parseAtomicOpStatement();
    std::unique_ptr<StatementNode> parseCohortOpStatement();
    std::unique_ptr<StatementNode> parseInquireStatement();
    std::unique_ptr<StatementNode> parseSyncStatement();
    std::unique_ptr<StatementNode> parseInterpolateStatement();

    std::unique_ptr<ExpressionNode> parseExpression();
    std::unique_ptr<ExpressionNode> parseLogicalOr();
    std::unique_ptr<ExpressionNode> parseLogicalAnd();
    std::unique_ptr<ExpressionNode> parseLogicalNot();
    std::unique_ptr<ExpressionNode> parseComparison();
    std::unique_ptr<ExpressionNode> parseBitwise();
    std::unique_ptr<ExpressionNode> parseShift();
    std::unique_ptr<ExpressionNode> parseAddition();
    std::unique_ptr<ExpressionNode> parseMultiplication();
    std::unique_ptr<ExpressionNode> parseUnary();
    std::unique_ptr<ExpressionNode> parsePrimary();
};

} // namespace cobolv
