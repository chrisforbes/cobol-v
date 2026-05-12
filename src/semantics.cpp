#include "semantics.hpp"
#include "builtins.hpp"
#include <stdexcept>
#include <algorithm>
#include <set>
#include <iostream>

namespace cobolv {

void SemanticAnalyzer::analyze(const ProgramNode& program) {
    symbolTable.clear();
    requiredCapabilities.clear();
    shaderStage = program.shaderStage;
    
    // Pass 1: Process ENVIRONMENT DIVISION (SPECIAL-NAMES)
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::ENVIRONMENT) {
            visitDivision(*div);
        }
    }

    // Pass 2: Collect all data definitions (DATA DIVISION and INPUT-OUTPUT SECTION)
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::DATA || div->divisionType == TokenType::INPUT_OUTPUT) {
            visitDivision(*div);
        }
    }

    // Pass 3: Process PROCEDURE DIVISION (Statements)
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::PROCEDURE) {
            visitDivision(*div);
        }
    }
}

void SemanticAnalyzer::visitDivision(const DivisionNode& division) {
    for (const auto& section : division.sections) {
        visitSection(*section);
    }
}

void SemanticAnalyzer::visitSection(const SectionNode& section) {
    currentSection = section.getSectionType();
    for (const auto& child : section.children) {
        visitNode(*child);
    }
}

void SemanticAnalyzer::visitNode(const AstNode& node) {
    if (auto select = dynamic_cast<const SelectNode*>(&node)) visitSelect(*select);
    else if (auto fd = dynamic_cast<const FileDescriptionNode*>(&node)) visitFileDescription(*fd);
    else if (auto item = dynamic_cast<const DataItemNode*>(&node)) visitDataItem(*item);
    else if (auto stmt = dynamic_cast<const StatementNode*>(&node)) visitStatement(*stmt);
    else if (auto spec = dynamic_cast<const SpecialNamesNode*>(&node)) {
        localSize = spec->localSize;
        for (auto const& [hw, cob] : spec->builtInMappings) {
            if (builtInTable.count(hw)) {
                const auto& info = builtInTable.at(hw);
                if (info.stageInterfaces.count(shaderStage) == 0) {
                    throw std::runtime_error("Hardware built-in '" + hw + "' is not supported in the current shader stage");
                }
            }
            
            symbolTable[cob].isBuiltIn = true;
            symbolTable[cob].hardwareName = hw;
            
            if (hw == "GPU-SUBGROUP-ID" || hw == "GPU-SUBGROUP-SIZE") {
                requiredCapabilities.insert(spirv::CapabilityGroupNonUniform);
            }
        }
    }
}

void SemanticAnalyzer::visitSelect(const SelectNode& select) {
    Symbol sym;
    sym.name = select.name;
    sym.isResource = true;
    sym.resourceInfo.organization = select.organization;
    sym.resourceInfo.accessMode = select.accessMode;
    symbolTable[select.name] = sym;

    if (select.organization >= OrganizationType::IMAGE_1D && 
        select.organization <= OrganizationType::STORAGE_CUBE_ARRAY) {
        // For now, assume Shader is enough
    }
}

void SemanticAnalyzer::visitFileDescription(const FileDescriptionNode& fd) {
    if (!symbolTable.count(fd.name)) {
        error("FD for undeclared file: " + fd.name, 0, 0);
    }
    auto& sym = symbolTable[fd.name];
    sym.resourceInfo.format = fd.format;
    sym.resourceInfo.coherent = fd.isCoherent;
    sym.resourceInfo.volatile_ = fd.isVolatile;
    if (fd.records.empty()) {
        error("FD entry for '" + fd.name + "' must have at least one record definition.", 0, 0);
    } else {
        sym.resourceInfo.recordType = fd.records[0]->dataType;
        sym.type = sym.resourceInfo.recordType; // Sampled type
        sym.resourceInfo.sampledType = sym.resourceInfo.recordType.baseType;
    }
    
    for (const auto& record : fd.records) {
        visitNode(*record);
    }



    if (fd.layout == LayoutType::SCALAR) {
        OrganizationType org = sym.resourceInfo.organization;
        if (org != OrganizationType::ACCESS_STORAGE) {
            error("SCALAR layout is only valid for ACCESS-STORAGE blocks", 0, 0);
        }
    }
}

void SemanticAnalyzer::visitDataItem(const DataItemNode& item) {
    if (currentSection == SectionType::LOCAL_STORAGE && item.initialValue) {
        throw std::runtime_error("Variables in LOCAL-STORAGE SECTION cannot have initial values: " + item.name);
    }

    Symbol& sym = symbolTable[item.name];
    sym.name = item.name;
    sym.type = item.dataType;
    sym.occursCount = item.occursCount;
    if (item.isBuiltIn) sym.isBuiltIn = true;
    sym.location = item.location;
    sym.interpolation = item.interpolation;
    
    if (item.isInput) sym.storageClass = spirv::StorageClassInput;
    else if (item.isOutput) sym.storageClass = spirv::StorageClassOutput;
    else if (currentSection == SectionType::WORKING_STORAGE) sym.storageClass = spirv::StorageClassPrivate;
    else if (currentSection == SectionType::LOCAL_STORAGE) sym.storageClass = spirv::StorageClassWorkgroup;
    else if (currentSection == SectionType::LINKAGE) sym.storageClass = spirv::StorageClassUniform; 

    // Validate Built-in interface
    if (sym.isBuiltIn && !sym.hardwareName.empty()) {
        if (builtInTable.count(sym.hardwareName)) {
            const auto& info = builtInTable.at(sym.hardwareName);
            if (!info.isConstant) {
                if (info.stageInterfaces.count(shaderStage)) {
                    spirv::StorageClass requiredSC = info.stageInterfaces.at(shaderStage);
                    if (sym.storageClass != requiredSC && 
                        sym.storageClass != spirv::StorageClassPrivate &&
                        sym.storageClass != spirv::StorageClassUniform) {
                        std::string requiredSection = (requiredSC == spirv::StorageClassInput) ? "INPUT SECTION" : "OUTPUT SECTION";
                        throw std::runtime_error("Hardware built-in '" + sym.hardwareName + "' must be declared in the " + requiredSection + ", WORKING-STORAGE SECTION, or LINKAGE SECTION");
                    }
                }
            }
        }
    }

    // Validate Interpolation
    if (sym.interpolation != InterpolationMode::NONE) {
        if (shaderStage != ShaderStage::FRAGMENT) {
            throw std::runtime_error("Interpolation qualifiers are only valid in FRAGMENT shaders: " + item.name);
        }
        if (sym.storageClass != spirv::StorageClassInput) {
            throw std::runtime_error("Interpolation qualifiers are only valid on INPUT variables: " + item.name);
        }
    }

    if (item.dataType.matrixRows > 0) {
        requiredCapabilities.insert(spirv::CapabilityMatrix);
    }
    
    for (const auto& child : item.children) {
        visitDataItem(*child);
    }
}

void SemanticAnalyzer::visitStatement(const StatementNode& stmt) {
    if (auto read = dynamic_cast<const ReadNode*>(&stmt)) validateRead(*read);
    else if (auto write = dynamic_cast<const WriteNode*>(&stmt)) validateWrite(*write);
    else if (auto atomic = dynamic_cast<const AtomicOpNode*>(&stmt)) validateAtomicOp(*atomic);
    else if (auto cohort = dynamic_cast<const CohortOpNode*>(&stmt)) validateCohortOp(*cohort);
    else if (auto ifStmt = dynamic_cast<const IfStatementNode*>(&stmt)) {
        auto condType = getExpressionType(*ifStmt->condition);
        if (condType.baseType != BaseType::BOOL) {
            throw std::runtime_error("IF condition must be a boolean expression");
        }
        for (const auto& s : ifStmt->thenBranch) visitStatement(*s);
        for (const auto& s : ifStmt->elseBranch) visitStatement(*s);
    }
    else if (auto perf = dynamic_cast<const PerformStatementNode*>(&stmt)) {
        if (perf->varyingVar) {
            getExpressionType(*perf->varyingVar);
            getExpressionType(*perf->fromExpr);
            getExpressionType(*perf->byExpr);
        }
        if (perf->untilExpr) {
            auto condType = getExpressionType(*perf->untilExpr);
            if (condType.baseType != BaseType::BOOL) {
                throw std::runtime_error("PERFORM UNTIL condition must be a boolean expression");
            }
        }
        for (const auto& s : perf->body) visitStatement(*s);
    }
    else if (auto move = dynamic_cast<const MoveNode*>(&stmt)) validateMove(*move);
    else if (auto call = dynamic_cast<const CallNode*>(&stmt)) validateCall(*call);
    else if (auto sync = dynamic_cast<const SyncStatementNode*>(&stmt)) validateSync(*sync);
    else if (auto inquire = dynamic_cast<const InquireStatementNode*>(&stmt)) validateInquire(*inquire);
    else if (auto discard = dynamic_cast<const DiscardNode*>(&stmt)) validateDiscard(*discard);
    else if (auto comp = dynamic_cast<const ComputeNode*>(&stmt)) {
        DataType src = getExpressionType(*comp->expression);
        DataType dst = getExpressionType(*comp->destination);

        const ExpressionNode* dest = comp->destination.get();
        while (auto swiz = dynamic_cast<const SwizzleNode*>(dest)) dest = swiz->inner.get();
        if (auto ident = dynamic_cast<const IdentifierNode*>(dest)) {
            const auto& sym = getSymbol(ident->name);
            if (sym.isBuiltIn) {
                bool canWrite = false;
                if (sym.storageClass == spirv::StorageClassOutput) {
                    canWrite = true;
                }
                if (!canWrite) {
                    error("Cannot assign to read-only built-in variable: " + ident->name, 0, 0);
                }
            }
        }

        if (!areTypesCompatible(src, dst)) {
            error("Type mismatch in COMPUTE. Source not compatible with destination.", 0, 0);
        }
    }
}

void SemanticAnalyzer::validateRead(const ReadNode& read) {
    if (!symbolTable.count(read.imageName)) {
        error("Read from undeclared resource: " + read.imageName, 0, 0); 
    }
    const auto& sym = getSymbol(read.imageName);
    
    // Check Coordinates
    DataType coordType = getExpressionType(*read.coordinates);
    OrganizationType org = sym.resourceInfo.organization;

    if (read.isFetch) {
        if (coordType.baseType != BaseType::INT) {
            error("FETCH requires integer coordinates.", 0, 0);
        }
    }

    if (read.isProjective) {
        if (read.isFetch || read.isGather) {
            error("PROJECTION cannot be used with FETCH or GATHER.", 0, 0);
        }
        if (org == OrganizationType::IMAGE_CUBE || org == OrganizationType::TEXTURE_CUBE || org == OrganizationType::STORAGE_CUBE ||
            org == OrganizationType::IMAGE_CUBE_ARRAY || org == OrganizationType::TEXTURE_CUBE_ARRAY || org == OrganizationType::STORAGE_CUBE_ARRAY) {
            error("Projective texturing is not supported for CUBE images.", 0, 0);
        }
        int stdSize = getExpectedCoordinateSize(org);
        int expectedProjSize = (stdSize == 0) ? 2 : stdSize + 1;
        if (coordType.vectorSize != expectedProjSize) {
            error("Projective READ requires " + std::to_string(expectedProjSize) + " coordinates, but got " + std::to_string(coordType.vectorSize), 0, 0);
        }
    } else {
        int expectedSize = getExpectedCoordinateSize(org);
        if (coordType.vectorSize != expectedSize) {
            std::string expectedStr = (expectedSize == 0) ? "scalar" : "v" + std::to_string(expectedSize);
            error("Image read for this organization requires " + expectedStr + " coordinates, but got vector size " + std::to_string(coordType.vectorSize), 0, 0);
        }
    }

    // Check Target
    DataType targetType = getExpressionType(*read.target);
    
    if (read.isGather) {
        if (targetType.vectorSize != 4) {
            error("GATHER target must be a vec4.", 0, 0);
        }
        if (read.component) {
            getExpressionType(*read.component);
        }
    } else {
        if (read.compareValue) {
            if (targetType.vectorSize != 0) {
                error("READ with COMPARING must INTO a scalar target.", 0, 0);
            }
        } else if (!areTypesCompatible(sym.resourceInfo.recordType, targetType)) {
            error("Type mismatch in READ. Image record type (" + std::to_string((int)sym.resourceInfo.recordType.baseType) + ", v" + std::to_string(sym.resourceInfo.recordType.vectorSize) + 
                  ") does not match target (" + std::to_string((int)targetType.baseType) + ", v" + std::to_string(targetType.vectorSize) + ")", 0, 0);
        }
    }

    if (read.compareValue) {
        if (read.isFetch || read.isGather) {
            error("COMPARING cannot be used with FETCH or GATHER.", 0, 0);
        }
        DataType cv = getExpressionType(*read.compareValue);
        if (cv.vectorSize != 0 || cv.baseType != BaseType::FLOAT) {
            error("Comparison value must be a scalar float.", 0, 0);
        }
    }
    
    if (read.lod) {
        visitNode(*read.lod);
    }

    if (read.gradX || read.gradY) {
        if (!read.gradX || !read.gradY) {
            error("Both GRADIENTS (X and Y) must be provided.", 0, 0);
        }
        if (read.isFetch || read.isGather) {
            error("GRADIENTS cannot be used with FETCH or GATHER.", 0, 0);
        }
        if (read.lod) {
            error("Cannot specify both GRADIENTS and AT LOD in the same READ statement.", 0, 0);
        }
        DataType gx = getExpressionType(*read.gradX);
        DataType gy = getExpressionType(*read.gradY);
        int expectedGradDim = getExpectedGradientSize(org);
        if (gx.vectorSize != expectedGradDim || gy.vectorSize != expectedGradDim) {
            error("Gradients must match image dimensionality.", 0, 0);
        }
        if (gx.baseType != BaseType::FLOAT || gy.baseType != BaseType::FLOAT) {
            error("Gradients must be of float type.", 0, 0);
        }
    }
}

void SemanticAnalyzer::validateWrite(const WriteNode& write) {
    if (!symbolTable.count(write.imageName)) {
        error("Write to undeclared resource: " + write.imageName, 0, 0);
    }
    const auto& sym = getSymbol(write.imageName);
    if (sym.resourceInfo.organization < OrganizationType::STORAGE_1D || sym.resourceInfo.organization > OrganizationType::STORAGE_CUBE_ARRAY) {
        error("WRITE statement only supported for STORAGE resources.", 0, 0);
    }

    // Check Coordinates
    DataType coordType = getExpressionType(*write.coordinates);
    int expectedSize = getExpectedCoordinateSize(sym.resourceInfo.organization);
    if (coordType.vectorSize != expectedSize) {
        std::string expectedStr = (expectedSize == 0) ? "scalar" : "v" + std::to_string(expectedSize);
        error("Image write for this organization requires " + expectedStr + " coordinates, but got vector size " + std::to_string(coordType.vectorSize), 0, 0);
    }

    DataType sourceType = getExpressionType(*write.source);
    if (!areTypesCompatible(sourceType, sym.resourceInfo.recordType)) {
        error("Type mismatch in WRITE. Source type does not match image record format.", 0, 0);
    }
}

void SemanticAnalyzer::validateAtomicOp(const AtomicOpNode& atomic) {
    DataType targetType = {BaseType::UNKNOWN, 0, 0, 0};
    bool isImage = false;
    OrganizationType org = OrganizationType::UNKNOWN;

    if (auto ident = dynamic_cast<const IdentifierNode*>(atomic.target.get())) {
        if (symbolTable.count(ident->name)) {
            const auto& sym = getSymbol(ident->name);
            if (sym.isResource && sym.resourceInfo.organization >= OrganizationType::STORAGE_1D && sym.resourceInfo.organization <= OrganizationType::STORAGE_CUBE_ARRAY) {
                isImage = true;
                org = sym.resourceInfo.organization;
                targetType = {sym.resourceInfo.sampledType, 0, 0, 0};
            }
        }
    }

    if (!isImage) {
        targetType = getExpressionType(*atomic.target);
    }

    if (isImage) {
        if (!atomic.coordinates) {
            error("Atomic operation on image requires AT [coords] clause.", 0, 0);
        }
        DataType coordType = getExpressionType(*atomic.coordinates);
        int expectedSize = getExpectedCoordinateSize(org);
        if (coordType.vectorSize != expectedSize) {
            std::string expectedStr = (expectedSize == 0) ? "scalar" : "v" + std::to_string(expectedSize);
            error("Atomic operation for this organization requires " + expectedStr + " coordinates, but got vector size " + std::to_string(coordType.vectorSize), 0, 0);
        }
    } else {
        if (atomic.coordinates) {
            error("Atomic operation on non-image target does not support AT [coords] clause.", 0, 0);
        }
    }

    if (targetType.baseType != BaseType::INT && targetType.baseType != BaseType::UINT) {
        error("Atomic operations require integer target.", 0, 0);
    }
    
    if (targetType.vectorSize != 0) {
        error("Atomic operations only supported for scalar integer targets.", 0, 0);
    }

    DataType valType = getExpressionType(*atomic.value);
    if (valType.baseType != targetType.baseType) {
        if (!((valType.baseType == BaseType::INT || valType.baseType == BaseType::UINT) &&
              (targetType.baseType == BaseType::INT || targetType.baseType == BaseType::UINT))) {
            error("Value type must match target type in atomic operation.", 0, 0);
        }
    }
    
    if (atomic.comparator) {
        DataType compType = getExpressionType(*atomic.comparator);
        if (compType.baseType != targetType.baseType) {
            if (!((compType.baseType == BaseType::INT || compType.baseType == BaseType::UINT) &&
                  (targetType.baseType == BaseType::INT || targetType.baseType == BaseType::UINT))) {
                error("Comparator type must match target type in ATOMIC-COMPARE-EXCHANGE.", 0, 0);
            }
        }
    }
    
    if (atomic.destination) {
        DataType destType = getExpressionType(*atomic.destination);
        if (!areTypesCompatible(targetType, destType)) {
            error("GIVING target type is not compatible with atomic operation result type.", 0, 0);
        }
    }
}

void SemanticAnalyzer::validateCohortOp(const CohortOpNode& cohort) {
    requiredCapabilities.insert(spirv::CapabilityGroupNonUniform);
    
    DataType valType = {BaseType::UNKNOWN, 0, 0, 0};
    if (cohort.value) {
        valType = getExpressionType(*cohort.value);
    }

    switch (cohort.opType) {
        case CohortOpType::SUM:
        case CohortOpType::PRODUCT:
        case CohortOpType::MIN:
        case CohortOpType::MAX:
        case CohortOpType::AND:
        case CohortOpType::OR:
        case CohortOpType::XOR:
            requiredCapabilities.insert(spirv::CapabilityGroupNonUniformArithmetic);
            break;
        case CohortOpType::BALLOT:
            requiredCapabilities.insert(spirv::CapabilityGroupNonUniformBallot);
            break;
        case CohortOpType::BROADCAST:
        case CohortOpType::BROADCAST_FIRST:
            requiredCapabilities.insert(spirv::CapabilityGroupNonUniformBallot);
            break;
        case CohortOpType::ANY:
        case CohortOpType::ALL:
            requiredCapabilities.insert(spirv::CapabilityGroupNonUniformVote);
            break;
        case CohortOpType::ELECT:
            break;
    }

    DataType destType = getExpressionType(*cohort.destination);
    
    if (cohort.opType == CohortOpType::ELECT || cohort.opType == CohortOpType::ANY || cohort.opType == CohortOpType::ALL) {
        if (destType.baseType != BaseType::BOOL) {
            error("Cohort query (ELECT, ANY, ALL) requires boolean target.", 0, 0);
        }
    } else if (cohort.opType == CohortOpType::BALLOT) {
        if (destType.baseType != BaseType::UINT || destType.vectorSize != 4) {
            error("Cohort BALLOT requires PIC UV(4) target.", 0, 0);
        }
    } else {
        if (!areTypesCompatible(valType, destType)) {
             error("Cohort operation result type is not compatible with GIVING target.", 0, 0);
        }
    }
}

void SemanticAnalyzer::validateMove(const MoveNode& move) {
    DataType src = getExpressionType(*move.source);
    DataType dst = getExpressionType(*move.destination);

    const ExpressionNode* dest = move.destination.get();
    while (auto swiz = dynamic_cast<const SwizzleNode*>(dest)) dest = swiz->inner.get();
    if (auto ident = dynamic_cast<const IdentifierNode*>(dest)) {
        const auto& sym = getSymbol(ident->name);
        if (sym.isBuiltIn) {
            bool canWrite = false;
            if (sym.storageClass == spirv::StorageClassOutput) {
                canWrite = true;
            }
            if (!canWrite) {
                error("Cannot assign to read-only built-in variable: " + ident->name, 0, 0);
            }
        }
    }

    if (!areTypesCompatible(src, dst)) {
        error("Type mismatch in MOVE/COMPUTE. Source not compatible with destination.", 0, 0);
    }
}

bool SemanticAnalyzer::isRelational(TokenType op) {
    return op == TokenType::EQUALS || op == TokenType::EQUAL ||
           op == TokenType::NOT_EQUAL_SYM || op == TokenType::NOT_EQUAL ||
           op == TokenType::GREATER_SYM || op == TokenType::GREATER ||
           op == TokenType::LESS_SYM || op == TokenType::LESS ||
           op == TokenType::GREATER_EQUAL_SYM || op == TokenType::GREATER_EQUAL ||
           op == TokenType::LESS_EQUAL_SYM || op == TokenType::LESS_EQUAL;
}

bool SemanticAnalyzer::isBitwise(TokenType op) {
    return op == TokenType::BIT_AND || op == TokenType::BIT_OR || op == TokenType::BIT_XOR ||
           op == TokenType::BIT_LSHIFT || op == TokenType::BIT_RSHIFT;
}

DataType SemanticAnalyzer::getExpressionType(const ExpressionNode& expr) {
    if (auto ident = dynamic_cast<const IdentifierNode*>(&expr)) {
        const auto& sym = getSymbol(ident->name);
        if (ident->subscript) {
            if (sym.occursCount == 0) {
                error("Cannot subscript non-array variable: " + ident->name, 0, 0);
            }
            DataType subType = getExpressionType(*ident->subscript);
            if (subType.baseType != BaseType::INT && subType.baseType != BaseType::UINT) {
                error("Subscript must be an integer.", 0, 0);
            }
            DataType type = sym.type;
            type.vectorSize = 0; // It's an element access
            return type;
        }
        return sym.type;
    }
    if (auto qident = dynamic_cast<const QualifiedIdentifierNode*>(&expr)) {
        const auto& sym = getSymbol(qident->name);
        if (qident->subscript) {
            if (sym.occursCount == 0) {
                error("Cannot subscript non-array member: " + qident->name, 0, 0);
            }
            DataType subType = getExpressionType(*qident->subscript);
            if (subType.baseType != BaseType::INT && subType.baseType != BaseType::UINT) {
                error("Subscript must be an integer.", 0, 0);
            }
            DataType type = sym.type;
            type.vectorSize = 0; // It's an element access
            return type;
        }
        return sym.type;
    }
    if (auto lit = dynamic_cast<const LiteralNode*>(&expr)) {
        DataType type = {BaseType::FLOAT, 0, 0, 0};
        if (lit->token.type == TokenType::NUMBER && lit->token.lexeme.find('.') == std::string::npos) type.baseType = BaseType::INT;
        return type;
    }
    if (auto swiz = dynamic_cast<const SwizzleNode*>(&expr)) {
        DataType varType = getExpressionType(*swiz->inner);
        DataType resType = varType;
        resType.vectorSize = (int)swiz->components.size();
        if (resType.vectorSize == 1) resType.vectorSize = 0;
        return resType;
    }
    if (auto vec = dynamic_cast<const VectorLiteralNode*>(&expr)) {
        DataType res = {BaseType::UNKNOWN, 0, 0, 0};
        int totalSize = 0;
        for (const auto& el : vec->elements) {
            DataType et = getExpressionType(*el);
            if (res.baseType == BaseType::UNKNOWN) res.baseType = et.baseType;
            totalSize += (et.vectorSize == 0 ? 1 : et.vectorSize);
        }
        res.vectorSize = (totalSize == 1 ? 0 : totalSize);
        return res;
    }
    if (auto unary = dynamic_cast<const UnaryExpressionNode*>(&expr)) {
        if (unary->op == TokenType::NOT) return {BaseType::BOOL, 0, 0, 0};
        DataType inner = getExpressionType(*unary->expr);
        if (unary->op == TokenType::BIT_NOT) {
            if (inner.baseType != BaseType::INT && inner.baseType != BaseType::UINT) {
                error("BIT-NOT requires integer operand.", 0, 0);
            }
        }
        return inner;
    }
    if (auto binary = dynamic_cast<const BinaryExpressionNode*>(&expr)) {
        if (isRelational(binary->op) || binary->op == TokenType::AND || binary->op == TokenType::OR) {
            return {BaseType::BOOL, 0, 0, 0};
        }
        DataType left = getExpressionType(*binary->left);
        DataType right = getExpressionType(*binary->right);

        if (binary->op == TokenType::STAR) {
            // Matrix-Vector Multiplication
            if (left.matrixRows > 0 && right.matrixRows == 0 && right.vectorSize > 0) {
                if (left.matrixCols != (right.vectorSize == 0 ? 1 : right.vectorSize)) {
                    error("Matrix column count must match vector size for multiplication.", 0, 0);
                }
                return {left.baseType, left.matrixRows, 0, 0};
            }
            // Vector-Matrix Multiplication
            if (left.matrixRows == 0 && left.vectorSize > 0 && right.matrixRows > 0) {
                if (left.vectorSize != right.matrixRows) {
                    error("Vector size must match matrix row count for multiplication.", 0, 0);
                }
                return {left.baseType, right.matrixCols, 0, 0};
            }
            // Matrix-Matrix Multiplication
            if (left.matrixRows > 0 && right.matrixRows > 0) {
                if (left.matrixCols != right.matrixRows) {
                    error("Matrix inner dimensions must match for multiplication.", 0, 0);
                }
                return {left.baseType, 0, left.matrixRows, right.matrixCols};
            }
        }

        if (isBitwise(binary->op)) {
            if (left.baseType != BaseType::INT && left.baseType != BaseType::UINT) {
                error("Bitwise operation requires integer operands.", 0, 0);
            }
            if (right.baseType != BaseType::INT && right.baseType != BaseType::UINT) {
                error("Bitwise operation requires integer operands.", 0, 0);
            }
        }
        return left; 
    }
    return {BaseType::UNKNOWN, 0, 0, 0};
}

static std::set<std::string> supportedGlslOps = {
    "ROUND", "TRUNC", "ABS", "SIGN", "FLOOR", "CEIL", "FRACT",
    "SIN", "COS", "TAN", "ASIN", "ACOS", "ATAN",
    "POW", "EXP", "LOG", "SQRT", "INVERSESQRT",
    "MIN", "MAX", "CLAMP", "MIX", "STEP", "SMOOTHSTEP",
    "LENGTH", "DISTANCE", "CROSS", "NORMALIZE", "FACEFORWARD",
    "REFLECT", "REFRACT",
    "FREXP", "MODF"
};

static std::set<std::string> derivativeOps = {
    "DERIVATIVE-X", "DERIVATIVE-Y", "TOTAL-DERIVATIVE",
    "DERIVATIVE-X-FINE", "DERIVATIVE-Y-FINE", "TOTAL-DERIVATIVE-FINE",
    "DERIVATIVE-X-COARSE", "DERIVATIVE-Y-COARSE", "TOTAL-DERIVATIVE-COARSE"
};

void SemanticAnalyzer::validateCall(const CallNode& call) {
    std::string fullName = call.functionName;
    std::string name = fullName;
    size_t lastDot = name.find_last_of('.');
    bool hasQualifier = (lastDot != std::string::npos);
    if (hasQualifier) name = name.substr(lastDot + 1);
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

    std::cerr << "Validating call: " << fullName << " (name: " << name << ", hasQualifier: " << hasQualifier << ")" << std::endl;

    if (!hasQualifier && derivativeOps.count(name)) {
        if (shaderStage != ShaderStage::FRAGMENT) {
            error("Derivative instructions are only valid in the FRAGMENT stage.", 0, 0);
        }
        if (call.arguments.size() != 1) {
            error("Derivative instructions expect exactly one argument.", 0, 0);
        }
        DataType argType = getExpressionType(*call.arguments[0]);
        if (argType.baseType != BaseType::FLOAT) {
            error("Derivative instructions require floating-point arguments.", 0, 0);
        }
    } else {
        if (supportedGlslOps.find(name) == supportedGlslOps.end()) {
            error("Unsupported GLSL instruction: " + name, 0, 0); 
        }
    }

    for (const auto& arg : call.arguments) {
        getExpressionType(*arg);
    }
    for (const auto& dest : call.destinations) {
        getExpressionType(*dest);
    }
}

bool SemanticAnalyzer::areTypesCompatible(const DataType& source, const DataType& target) {
    if (source.baseType != target.baseType) return false;
    if (source.matrixRows != target.matrixRows) return false;
    if (source.matrixCols != target.matrixCols) return false;
    if (source.vectorSize == target.vectorSize) return true;
    if (source.vectorSize == 0) return true; // Scalar can splat to vector
    return false;
}

int SemanticAnalyzer::getExpectedCoordinateSize(OrganizationType org) {
    if (org == OrganizationType::IMAGE_1D || org == OrganizationType::TEXTURE_1D || org == OrganizationType::STORAGE_1D) return 0; // scalar
    if (org == OrganizationType::IMAGE_2D || org == OrganizationType::TEXTURE_2D || org == OrganizationType::STORAGE_2D) return 2;
    if (org == OrganizationType::IMAGE_3D || org == OrganizationType::TEXTURE_3D || org == OrganizationType::STORAGE_3D) return 3;
    if (org == OrganizationType::IMAGE_1D_ARRAY || org == OrganizationType::TEXTURE_1D_ARRAY || org == OrganizationType::STORAGE_1D_ARRAY) return 2;
    if (org == OrganizationType::IMAGE_2D_ARRAY || org == OrganizationType::TEXTURE_2D_ARRAY || org == OrganizationType::STORAGE_2D_ARRAY) return 3;
    if (org == OrganizationType::IMAGE_CUBE || org == OrganizationType::TEXTURE_CUBE || org == OrganizationType::STORAGE_CUBE) return 3;
    if (org == OrganizationType::IMAGE_CUBE_ARRAY || org == OrganizationType::TEXTURE_CUBE_ARRAY || org == OrganizationType::STORAGE_CUBE_ARRAY) return 4;
    return 2; // Default
}

void SemanticAnalyzer::validateSync(const SyncStatementNode& sync) {
    // Basic validation: SYNC WORKGROUP is always valid in our compute shaders for now.
}


int SemanticAnalyzer::getExpectedGradientSize(OrganizationType org) {
    if (org == OrganizationType::IMAGE_1D || org == OrganizationType::TEXTURE_1D || org == OrganizationType::STORAGE_1D) return 0;
    if (org == OrganizationType::IMAGE_1D_ARRAY || org == OrganizationType::TEXTURE_1D_ARRAY || org == OrganizationType::STORAGE_1D_ARRAY) return 0;
    if (org == OrganizationType::IMAGE_2D || org == OrganizationType::TEXTURE_2D || org == OrganizationType::STORAGE_2D) return 2;
    if (org == OrganizationType::IMAGE_2D_ARRAY || org == OrganizationType::TEXTURE_2D_ARRAY || org == OrganizationType::STORAGE_2D_ARRAY) return 2;
    if (org == OrganizationType::IMAGE_3D || org == OrganizationType::TEXTURE_3D || org == OrganizationType::STORAGE_3D) return 3;
    if (org == OrganizationType::IMAGE_CUBE || org == OrganizationType::TEXTURE_CUBE || org == OrganizationType::STORAGE_CUBE) return 3;
    if (org == OrganizationType::IMAGE_CUBE_ARRAY || org == OrganizationType::TEXTURE_CUBE_ARRAY || org == OrganizationType::STORAGE_CUBE_ARRAY) return 3;
    return 2;
}

void SemanticAnalyzer::validateInquire(const InquireStatementNode& inquire) {
    const auto& sym = getSymbol(inquire.resourceName);
    
    if (inquire.lod) {
        visitNode(*inquire.lod);
        if (static_cast<int>(sym.resourceInfo.organization) >= static_cast<int>(OrganizationType::STORAGE_1D) && 
            static_cast<int>(sym.resourceInfo.organization) <= static_cast<int>(OrganizationType::STORAGE_CUBE_ARRAY)) {
             error("AT LOD clause not supported for STORAGE resource queries.", 0, 0);
        }
    }
    
    // Check destination type matches dimensions
    DataType destType = getExpressionType(*inquire.destination);
    
    if (inquire.queryType == InquireQueryType::SIZE) {
        int dims = getExpectedCoordinateSize(sym.resourceInfo.organization);
        
        int expectedResultSize = dims;
        if (sym.resourceInfo.organization == OrganizationType::IMAGE_CUBE || sym.resourceInfo.organization == OrganizationType::TEXTURE_CUBE || sym.resourceInfo.organization == OrganizationType::STORAGE_CUBE) {
            expectedResultSize = 2; // Cube is 2D size
        } else if (sym.resourceInfo.organization == OrganizationType::IMAGE_CUBE_ARRAY || sym.resourceInfo.organization == OrganizationType::TEXTURE_CUBE_ARRAY || sym.resourceInfo.organization == OrganizationType::STORAGE_CUBE_ARRAY) {
            expectedResultSize = 3; // CubeArray is 2D size + layers
        }

        if (destType.vectorSize != expectedResultSize) {
            std::string expectedStr = (expectedResultSize == 0) ? "scalar" : "v" + std::to_string(expectedResultSize);
            error("INQUIRE ... FOR SIZE on this resource requires " + expectedStr + " target, but got vector size " + std::to_string(destType.vectorSize), 0, 0);
        }
    } else if (inquire.queryType == InquireQueryType::LEVELS) {
        if (inquire.lod) {
            error("AT LOD clause not supported for LEVELS query.", 0, 0);
        }
        if (destType.vectorSize != 0 || destType.baseType != BaseType::INT) {
            error("INQUIRE ... FOR LEVELS requires a scalar integer target.", 0, 0);
        }
    }
}

void SemanticAnalyzer::validateDiscard(const DiscardNode& discard) {
    if (shaderStage != ShaderStage::FRAGMENT) {
        error("DISCARD statement is only valid in the FRAGMENT stage.", 0, 0);
    }
}

void SemanticAnalyzer::error(const std::string& message, int line, int column) {
    std::string loc = "";
    if (line > 0) loc = " (line " + std::to_string(line) + ", column " + std::to_string(column) + ")";
    throw std::runtime_error("Semantic Error: " + message + loc);
}

const Symbol& SemanticAnalyzer::getSymbol(const std::string& name) {
    if (symbolTable.count(name)) return symbolTable.at(name);
    error("Undeclared identifier: " + name, 0, 0);
    throw std::runtime_error("Unreachable");
}

} // namespace cobolv
