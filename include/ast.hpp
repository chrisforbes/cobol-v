#pragma once

#include <string>
#include <vector>
#include <memory>
#include "lexer.hpp"

namespace cobolv {

enum class AstNodeType {
    PROGRAM,
    DIVISION,
    SECTION,
    STATEMENT,
    EXPRESSION,
    DATA_ITEM,
    SELECT,
    FILE_DESCRIPTION
};

class AstNode {
public:
    virtual ~AstNode() = default;
    virtual AstNodeType getType() const = 0;
};

class ExpressionNode : public AstNode {
public:
    AstNodeType getType() const override { return AstNodeType::EXPRESSION; }
    virtual ~ExpressionNode() = default;
};

class LiteralNode : public ExpressionNode {
public:
    Token token;
    LiteralNode(Token t) : token(t) {}
};

class IdentifierNode : public ExpressionNode {
public:
    std::string name;
    std::unique_ptr<ExpressionNode> subscript;
    IdentifierNode(std::string n) : name(n) {}
};

class QualifiedIdentifierNode : public ExpressionNode {
public:
    std::string name;
    std::unique_ptr<ExpressionNode> parent;
    std::unique_ptr<ExpressionNode> subscript;
    QualifiedIdentifierNode(std::string n, std::unique_ptr<ExpressionNode> p) : name(n), parent(std::move(p)) {}
};

class SwizzleNode : public ExpressionNode {
public:
    std::vector<int> components;
    std::unique_ptr<ExpressionNode> inner;
    SwizzleNode(std::vector<int> c, std::unique_ptr<ExpressionNode> i) : components(c), inner(std::move(i)) {}
};

class BinaryExpressionNode : public ExpressionNode {
public:
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;
    TokenType op;
    BinaryExpressionNode(std::unique_ptr<ExpressionNode> l, TokenType o, std::unique_ptr<ExpressionNode> r)
        : left(std::move(l)), op(o), right(std::move(r)) {}
};

class UnaryExpressionNode : public ExpressionNode {
public:
    TokenType op;
    std::unique_ptr<ExpressionNode> expr;
    UnaryExpressionNode(TokenType o, std::unique_ptr<ExpressionNode> e)
        : op(o), expr(std::move(e)) {}
};

class VectorLiteralNode : public ExpressionNode {
public:
    std::vector<std::unique_ptr<ExpressionNode>> elements;
    VectorLiteralNode(std::vector<std::unique_ptr<ExpressionNode>> e) : elements(std::move(e)) {}
};

class ConversionNode : public ExpressionNode {
public:
    TokenType convType;
    std::unique_ptr<ExpressionNode> expr;
    ConversionNode(TokenType t, std::unique_ptr<ExpressionNode> e) : convType(t), expr(std::move(e)) {}
};

class StatementNode : public AstNode {
public:
    AstNodeType getType() const override { return AstNodeType::STATEMENT; }
};

class GoBackNode : public StatementNode {
};

class DiscardNode : public StatementNode {
};

class MoveNode : public StatementNode {
public:
    std::unique_ptr<ExpressionNode> source;
    std::unique_ptr<ExpressionNode> destination;
};

class ComputeNode : public StatementNode {
public:
    std::unique_ptr<ExpressionNode> destination;
    std::unique_ptr<ExpressionNode> expression;
};

class ReadNode : public StatementNode {
public:
    std::string imageName;
    std::string samplerName;
    std::unique_ptr<ExpressionNode> coordinates;
    std::unique_ptr<ExpressionNode> target;
    std::unique_ptr<ExpressionNode> lod; // Optional
    std::unique_ptr<ExpressionNode> lodBias; // Optional
    bool isFetch = false;
    bool isGather = false;
    std::unique_ptr<ExpressionNode> component; // For GATHER
    std::unique_ptr<ExpressionNode> gradX;     // For explicit gradients
    std::unique_ptr<ExpressionNode> gradY;     // For explicit gradients
    std::unique_ptr<ExpressionNode> compareValue; // For shadow/comparison sampling
    bool isProjective = false;                   // For projective texturing
};

class WriteNode : public StatementNode {
public:
    std::string imageName;
    std::unique_ptr<ExpressionNode> source;
    std::unique_ptr<ExpressionNode> coordinates;
};

enum class AtomicOpType {
    ADD, SUB, AND, OR, XOR, MIN, MAX, EXCHANGE, COMPARE_EXCHANGE
};

class AtomicOpNode : public StatementNode {
public:
    AtomicOpType opType;
    std::unique_ptr<ExpressionNode> target;
    std::unique_ptr<ExpressionNode> coordinates;
    std::unique_ptr<ExpressionNode> value;
    std::unique_ptr<ExpressionNode> comparator; // For CompareExchange
    std::unique_ptr<ExpressionNode> destination; // Optional result (old value)
};

enum class CohortOpType {
    SUM, PRODUCT, MIN, MAX, AND, OR, XOR,
    ANY, ALL, ELECT, BALLOT, BROADCAST, BROADCAST_FIRST
};

enum class CohortMode {
    REDUCE, INCLUSIVE_SCAN, EXCLUSIVE_SCAN
};

class CohortOpNode : public StatementNode {
public:
    CohortOpType opType;
    CohortMode mode;
    std::unique_ptr<ExpressionNode> value;       // The value to reduce/broadcast/ballot/any/all
    std::unique_ptr<ExpressionNode> sourceId;    // For BROADCAST
    std::unique_ptr<ExpressionNode> destination; // GIVING [target]
};

class CallNode : public StatementNode {
public:
    std::string functionName;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    std::vector<std::unique_ptr<ExpressionNode>> destinations;
};

class SyncStatementNode : public StatementNode {
public:
    enum class Scope { WORKGROUP };
    Scope scope;
    SyncStatementNode(Scope s) : scope(s) {}
};

enum class InquireQueryType { SIZE, LEVELS };

class InquireStatementNode : public StatementNode {
public:
    std::string resourceName;
    InquireQueryType queryType;
    std::unique_ptr<ExpressionNode> lod; // Optional
    std::unique_ptr<ExpressionNode> destination;
};

enum class InterpolateType {
    CENTROID, SAMPLE, OFFSET
};

class InterpolateNode : public StatementNode {
public:
    std::unique_ptr<ExpressionNode> interpolant;
    InterpolateType type;
    std::unique_ptr<ExpressionNode> sampleIndex; // For AT SAMPLE
    std::unique_ptr<ExpressionNode> offset;      // For AT OFFSET
    std::unique_ptr<ExpressionNode> destination;
};

class IfStatementNode : public StatementNode {
public:
    std::unique_ptr<ExpressionNode> condition;
    std::vector<std::unique_ptr<StatementNode>> thenBranch;
    std::vector<std::unique_ptr<StatementNode>> elseBranch;
};

class PerformStatementNode : public StatementNode {
public:
    // VARYING variant
    std::unique_ptr<ExpressionNode> varyingVar;
    std::unique_ptr<ExpressionNode> fromExpr;
    std::unique_ptr<ExpressionNode> byExpr;
    std::unique_ptr<ExpressionNode> untilExpr;

    // TIMES variant
    std::unique_ptr<ExpressionNode> timesExpr;

    std::vector<std::unique_ptr<StatementNode>> body;
};

enum class BaseType {
    FLOAT, INT, UINT, BOOL, UNKNOWN
};

enum class InterpolationMode {
    NONE, FLAT, NOPERSPECTIVE, SMOOTH
};

struct DataType {
    BaseType baseType = BaseType::UNKNOWN;
    int vectorSize = 0;
    int matrixRows = 0;
    int matrixCols = 0;
};

enum class ShaderStage {
    COMPUTE,
    VERTEX,
    FRAGMENT,
    UNKNOWN
};

class DataItemNode : public AstNode {
public:
    int level;
    std::string name;
    DataType dataType;
    std::unique_ptr<ExpressionNode> initialValue;
    bool isPointer = false;
    std::string pointerTo;
    bool isBuiltIn = false;
    int location = -1;
    InterpolationMode interpolation = InterpolationMode::NONE;
    bool isInput = false;
    bool isOutput = false;
    int occursCount = 0;
    std::vector<std::unique_ptr<DataItemNode>> children;

    AstNodeType getType() const override { return AstNodeType::DATA_ITEM; }
};


enum class SectionType {
    CONFIGURATION, INPUT_OUTPUT, FILE_SECTION, WORKING_STORAGE, LOCAL_STORAGE, LINKAGE, SPECIAL_NAMES, PROCEDURE,
    INPUT_SECTION, OUTPUT_SECTION
};

class SectionNode : public AstNode {
public:
    std::string name;
    std::vector<std::unique_ptr<AstNode>> children;
    virtual SectionType getSectionType() const = 0;
    AstNodeType getType() const override { return AstNodeType::SECTION; }
};

class GenericSectionNode : public SectionNode {
public:
    SectionType type;
    GenericSectionNode(SectionType t) : type(t) {}
    SectionType getSectionType() const override { return type; }
};

struct Int3 { int x, y, z; };

class SpecialNamesNode : public SectionNode {
public:
    Int3 localSize = {1, 1, 1};
    std::map<std::string, std::string> builtInMappings; // hardware name -> cobol ident
    SectionType getSectionType() const override { return SectionType::SPECIAL_NAMES; }
};

class DivisionNode : public AstNode {
public:
    TokenType divisionType;
    std::vector<std::unique_ptr<SectionNode>> sections;

    AstNodeType getType() const override { return AstNodeType::DIVISION; }
};

class EnvironmentDivisionNode : public DivisionNode {
public:
    EnvironmentDivisionNode() { divisionType = TokenType::ENVIRONMENT; }
};

enum class OrganizationType {
    IMAGE_1D, IMAGE_2D, IMAGE_3D, IMAGE_1D_ARRAY, IMAGE_2D_ARRAY, IMAGE_CUBE, IMAGE_CUBE_ARRAY,
    TEXTURE_1D, TEXTURE_2D, TEXTURE_3D, TEXTURE_1D_ARRAY, TEXTURE_2D_ARRAY, TEXTURE_CUBE, TEXTURE_CUBE_ARRAY,
    STORAGE_1D, STORAGE_2D, STORAGE_3D, STORAGE_1D_ARRAY, STORAGE_2D_ARRAY, STORAGE_CUBE, STORAGE_CUBE_ARRAY,
    SAMPLER, ACCESS_PUSH, ACCESS_UNIFORM, ACCESS_STORAGE, UNKNOWN
};

enum class LayoutType {
    STD140, STD430, SCALAR, DEFAULT
};

enum class AccessMode {
    READ_ONLY, WRITE_ONLY, READ_WRITE, UNKNOWN
};

class SelectNode : public AstNode {
public:
    std::string name;
    std::string assignTo;
    OrganizationType organization = OrganizationType::UNKNOWN;
    AccessMode accessMode = AccessMode::UNKNOWN;
    AstNodeType getType() const override { return AstNodeType::SELECT; }
};

class FileDescriptionNode : public AstNode {
public:
    std::string name;
    std::string format;
    bool isCoherent = false;
    bool isVolatile = false;
    LayoutType layout = LayoutType::DEFAULT;
    std::vector<std::unique_ptr<DataItemNode>> records;
    AstNodeType getType() const override { return AstNodeType::FILE_DESCRIPTION; }
};

class ProgramNode : public AstNode {
public:
    std::string programId;
    ShaderStage shaderStage = ShaderStage::COMPUTE;
    std::vector<std::unique_ptr<DivisionNode>> divisions;

    AstNodeType getType() const override { return AstNodeType::PROGRAM; }
};

} // namespace cobolv
