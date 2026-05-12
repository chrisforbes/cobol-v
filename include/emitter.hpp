#pragma once

#include "ast.hpp"
#include <spirv/unified1/spirv.hpp>
namespace spirv = spv;

#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <string>

#include "builtins.hpp"

namespace cobolv {

struct UniformMemberInfo {
    uint32_t index;
    uint32_t typeId;
    const DataItemNode* node;
};

struct UniformBlockInfo {
    uint32_t varId;
    uint32_t typeId;
    std::map<std::string, UniformMemberInfo> members; // name -> info
};

struct FDMeta {
    BaseType sampledType;
    std::string format;
    bool isCoherent;
    bool isVolatile;
};

class Emitter {
public:
    Emitter();
    std::vector<uint32_t> emit(const ProgramNode& program, const std::set<spirv::Capability>& capabilities = {});

private:
    uint32_t nextId = 1;
    
    std::vector<uint32_t> capabilityStream;
    std::vector<uint32_t> extensionStream;
    std::vector<uint32_t> importStream;
    std::vector<uint32_t> memoryModelStream;
    std::vector<uint32_t> entryStream;
    std::vector<uint32_t> debugStream;
    std::vector<uint32_t> annoStream;
    std::vector<uint32_t> typeStream;
    std::vector<uint32_t> bodyStream;
    
    std::vector<uint32_t>* currentStream = nullptr;
    std::vector<uint32_t> entryPointInterfaces;

    uint32_t allocateId();
    void emitOp(spirv::Op op, const std::vector<uint32_t>& operands = {});
    spirv::Op getProjectiveOp(spirv::Op baseOp, bool isProjective);
    void emitOpString(spirv::Op op, uint32_t id, const std::string& str);
    
    uint32_t voidTypeId = 0;
    uint32_t voidFuncTypeId = 0;
    uint32_t mainFuncId = 0;
    uint32_t extInstSetId = 0;
    bool inFunctionBody = false;

    Int3 localSize = {1, 1, 1};
    ShaderStage shaderStage = ShaderStage::COMPUTE;
    std::map<std::string, std::string> builtInMappings;
    std::set<spirv::Capability> currentCapabilities;

    std::map<std::string, uint32_t> variableIds;
    std::map<std::string, uint32_t> variableTypeIds;
    std::map<std::string, DataType> variableDataTypes;
    std::map<uint32_t, spirv::StorageClass> idStorageClasses;
    std::map<uint32_t, DataType> typeToDataType;
    std::map<uint32_t, uint32_t> arrayElementTypeIds;
    
    std::map<BaseType, uint32_t> baseTypeIds;
    std::map<std::string, uint32_t> typeCache;
    std::map<std::vector<uint32_t>, uint32_t> structTypeCache;
    std::map<std::string, uint32_t> constantIds;
    
    uint32_t samplerTypeId = 0;
    std::map<std::string, uint32_t> imageTypeCache;
    std::map<uint32_t, uint32_t> sampledImageTypeCache;
    std::map<std::string, OrganizationType> imageOrgs;

    uint32_t pushConstantId = 0;
    uint32_t pushConstantTypeId = 0;
    std::string pushConstantSelectName;
    std::map<std::string, UniformMemberInfo> pushConstantMembers;
    std::map<std::string, UniformBlockInfo> uniformBlocks;

    std::vector<std::pair<const PerformStatementNode*, uint32_t>> timesCounterIds;
    struct ImageMetadata {
        spirv::ImageFormat format;
        uint32_t sampledFlag; // 1=Sampled, 2=Storage
        uint32_t pixelTypeId;
        uint32_t imageTypeId;
        OrganizationType org;
    };
    std::map<std::string, ImageMetadata> imageMetadata;
    std::map<uint32_t, spirv::Decoration> blockDecs;
    bool usesImageAtomics = false;

    void scanForLoops(const ProgramNode& program);
    void scanStatements(const std::vector<std::unique_ptr<StatementNode>>& statements);

    uint32_t getOrCreateBaseType(BaseType type);
    uint32_t getOrCreateType(const DataType& type);
    uint32_t getOrCreateStructType(const std::vector<uint32_t>& memberTypeIds);
    uint32_t getOrCreatePointerType(uint32_t baseTypeId, spirv::StorageClass sc);
    uint32_t getOrCreateConstant(const LiteralNode& literal, uint32_t typeId);
    uint32_t getOrCreateConstant(int value, uint32_t typeId);
    uint32_t getOrCreateSampledImageType(uint32_t imageTypeId);

    uint32_t emitExpression(const ExpressionNode& expr, uint32_t targetTypeId = 0, bool wantAddress = false);
    void emitStatement(const StatementNode& stmt);
    void emitIfStatement(const IfStatementNode& node);
    void emitPerformStatement(const PerformStatementNode& node);
    void emitMove(const ExpressionNode* sourceExpr, const ExpressionNode* destExpr);
    void emitRead(const ReadNode* read);
    void emitWrite(const WriteNode* write);
    void emitAtomicOp(const AtomicOpNode* atomic);
    void emitCall(const CallNode* call);
    void emitCohortOp(const CohortOpNode* cohort);
    void emitSyncStatement(const SyncStatementNode* sync);
    void emitInquireStatement(const InquireStatementNode* inquire);
    void emitInterpolate(const InterpolateNode* interp);

    void emitCapabilities();
    void emitMemoryModel();
    void emitEntryPoints(const ProgramNode& program);
    void emitExecutionModes(const ProgramNode& program);
    void emitAnnotationsWithAccess(const ProgramNode& program);
    void emitInputOutput(const ProgramNode& program);
    void emitInputOutputWithAdvancedTypes(const ProgramNode& program, const std::map<std::string, FDMeta>& fdMetas);
    void emitTypesAndConstants(const ProgramNode& program);
    void emitFunctions(const ProgramNode& program);

    uint32_t getExpressionType(const ExpressionNode& expr);
    uint32_t emitBinaryExpression(const BinaryExpressionNode& binary, uint32_t expectedTypeId);
    uint32_t emitUnaryExpression(const UnaryExpressionNode& unary, uint32_t expectedTypeId);
    uint32_t emitVectorLiteral(const VectorLiteralNode& vec, uint32_t expectedTypeId);
    uint32_t emitConversion(const ConversionNode& conv);
    uint32_t emitSwizzle(const SwizzleNode& swizzle, bool asPointer);
    uint32_t splatScalar(uint32_t scalarId, uint32_t vectorTypeId, int size);

    void calculateStd430Layout(const DataType& type, uint32_t& size, uint32_t& align);
    void calculateStd140Layout(const DataType& type, uint32_t& size, uint32_t& align);
    void calculateScalarLayout(const DataType& type, uint32_t& size, uint32_t& align);
    bool isRelational(TokenType op);
    bool isBitwise(TokenType op);

    uint32_t lookupVariableId(const std::string& name);
    uint32_t lookupVariableTypeId(const std::string& name);
    bool isConstant(uint32_t id);
};

} // namespace cobolv
