#pragma once

#include "ast.hpp"
#include <spirv/unified1/spirv.hpp>
namespace spirv = spv;

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <set>

namespace cobolv {

struct ResourceInfo {
    OrganizationType organization;
    AccessMode accessMode;
    std::string format;
    DataType recordType;
    BaseType sampledType;
    bool coherent;
    bool volatile_;
};

struct Symbol {
    std::string name;
    DataType type;
    ResourceInfo resourceInfo;
    std::string hardwareName;
    bool isResource = false;
    bool isBuiltIn = false;
    int location = -1;
    InterpolationMode interpolation = InterpolationMode::NONE;
    int occursCount = 0;
    spirv::StorageClass storageClass = spirv::StorageClassMax;
};

class SemanticAnalyzer {
public:
    void analyze(const ProgramNode& program);
    std::set<spirv::Capability> getRequiredCapabilities() const { return requiredCapabilities; }
    ShaderStage getShaderStage() const { return shaderStage; }
    
    DataType getExpressionType(const ExpressionNode& expr);

private:
    std::map<std::string, Symbol> symbolTable;
    std::set<spirv::Capability> requiredCapabilities;
    Int3 localSize = {1, 1, 1};
    SectionType currentSection = SectionType::PROCEDURE;
    ShaderStage shaderStage = ShaderStage::COMPUTE;

    void visitDivision(const DivisionNode& division);
    void visitSection(const SectionNode& section);
    void visitNode(const AstNode& node);
    
    void visitSelect(const SelectNode& select);
    void visitFileDescription(const FileDescriptionNode& fd);
    void visitDataItem(const DataItemNode& item);
    void visitStatement(const StatementNode& stmt);
    
    void validateRead(const ReadNode& read);
    void validateWrite(const WriteNode& write);
    void validateAtomicOp(const AtomicOpNode& atomic);
    void validateCohortOp(const CohortOpNode& cohort);
    void validateMove(const MoveNode& move);
    void validateCall(const CallNode& call);
    void validateSync(const SyncStatementNode& sync);
    void validateInquire(const InquireStatementNode& inquire);
    void validateDiscard(const DiscardNode& discard);


    
    bool areTypesCompatible(const DataType& source, const DataType& target);
    bool isRelational(TokenType op);
    bool isBitwise(TokenType op);
    int getExpectedCoordinateSize(OrganizationType org);
    int getExpectedGradientSize(OrganizationType org);
    
    void error(const std::string& message, int line, int column);
    const Symbol& getSymbol(const std::string& name);
};

} // namespace cobolv
