#include "emitter.hpp"
#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <regex>
#include <functional>

namespace cobolv {

Emitter::Emitter() {}

uint32_t Emitter::allocateId() {
    return nextId++;
}

static spirv::ImageFormat stringToFormat(const std::string& format) {
    if (format == "RGBA32F") return spirv::ImageFormatRgba32f;
    if (format == "R32F") return spirv::ImageFormatR32f;
    if (format == "RGBA8") return spirv::ImageFormatRgba8;
    if (format == "R32I") return spirv::ImageFormatR32i;
    if (format == "R32UI") return spirv::ImageFormatR32ui;
    if (format == "RGBA16") return spirv::ImageFormatRgba16;
    if (format == "RGBA16F") return spirv::ImageFormatRgba16f;
    if (format == "R16F") return spirv::ImageFormatR16f;
    if (format == "RG32F") return spirv::ImageFormatRg32f;
    if (format == "RG16F") return spirv::ImageFormatRg16f;
    if (format == "R8") return spirv::ImageFormatR8;
    if (format == "R8Snorm") return spirv::ImageFormatR8Snorm;
    if (format == "RG8") return spirv::ImageFormatRg8;
    if (format == "RG8Snorm") return spirv::ImageFormatRg8Snorm;
    if (format == "RGBA8Snorm") return spirv::ImageFormatRgba8Snorm;
    if (format == "R16") return spirv::ImageFormatR16;
    if (format == "R16Snorm") return spirv::ImageFormatR16Snorm;
    if (format == "RG16") return spirv::ImageFormatRg16;
    if (format == "RG16Snorm") return spirv::ImageFormatRg16Snorm;
    if (format == "R32I") return spirv::ImageFormatR32i;
    if (format == "R32UI") return spirv::ImageFormatR32ui;
    if (format == "RG32I") return spirv::ImageFormatRg32i;
    if (format == "RG32UI") return spirv::ImageFormatRg32ui;
    if (format == "RGBA32I") return spirv::ImageFormatRgba32i;
    if (format == "RGBA32UI") return spirv::ImageFormatRgba32ui;
    if (format == "R16I") return spirv::ImageFormatR16i;
    if (format == "R16UI") return spirv::ImageFormatR16ui;
    if (format == "RG16I") return spirv::ImageFormatRg16i;
    if (format == "RG16UI") return spirv::ImageFormatRg16ui;
    if (format == "RGBA16I") return spirv::ImageFormatRgba16i;
    if (format == "RGBA16UI") return spirv::ImageFormatRgba16ui;
    if (format == "R8I") return spirv::ImageFormatR8i;
    if (format == "R8UI") return spirv::ImageFormatR8ui;
    if (format == "RG8I") return spirv::ImageFormatRg8i;
    if (format == "RG8UI") return spirv::ImageFormatRg8ui;
    if (format == "RGBA8I") return spirv::ImageFormatRgba8i;
    if (format == "RGBA8UI") return spirv::ImageFormatRgba8ui;
    if (format == "RGB10A2") return spirv::ImageFormatRgb10A2;
    if (format == "RGB10A2UI") return spirv::ImageFormatRgb10a2ui;
    if (format == "R11FG11FB10F") return spirv::ImageFormatR11fG11fB10f;
    
    return spirv::ImageFormatUnknown;
}

static bool isExtendedFormat(spirv::ImageFormat fmt) {
    switch (fmt) {
        case spirv::ImageFormatRgba32f:
        case spirv::ImageFormatRgba16f:
        case spirv::ImageFormatR32f:
        case spirv::ImageFormatRgba8:
        case spirv::ImageFormatRgba8Snorm:
        case spirv::ImageFormatRgba32i:
        case spirv::ImageFormatRgba16i:
        case spirv::ImageFormatRgba8i:
        case spirv::ImageFormatR32i:
        case spirv::ImageFormatRgba32ui:
        case spirv::ImageFormatRgba16ui:
        case spirv::ImageFormatRgba8ui:
        case spirv::ImageFormatR32ui:
        case spirv::ImageFormatUnknown:
            return false;
        default:
            return true;
    }
}

static std::map<std::string, uint32_t> glslOps = {
    {"ROUND", 1}, {"TRUNC", 3}, {"ABS", 4}, {"SIGN", 6}, {"FLOOR", 8}, {"CEIL", 9}, {"FRACT", 10},
    {"SIN", 13}, {"COS", 14}, {"TAN", 15}, {"ASIN", 16}, {"ACOS", 17}, {"ATAN", 18},
    {"POW", 26}, {"EXP", 27}, {"LOG", 28}, {"SQRT", 31}, {"INVERSESQRT", 32},
    {"MIN", 37}, {"MAX", 40}, {"CLAMP", 43}, {"MIX", 46}, {"STEP", 48}, {"SMOOTHSTEP", 49},
    {"LENGTH", 66}, {"DISTANCE", 67}, {"CROSS", 71}, {"NORMALIZE", 72}, {"FACEFORWARD", 73},
    {"REFLECT", 74}, {"REFRACT", 75},
    {"FREXP", 52}, {"MODF", 36} 
};

static std::map<std::string, uint32_t> coreOps = {
    {"DERIVATIVE-X", 207}, {"DERIVATIVE-Y", 208}, {"TOTAL-DERIVATIVE", 209},
    {"DERIVATIVE-X-FINE", 210}, {"DERIVATIVE-Y-FINE", 211}, {"TOTAL-DERIVATIVE-FINE", 212},
    {"DERIVATIVE-X-COARSE", 213}, {"DERIVATIVE-Y-COARSE", 214}, {"TOTAL-DERIVATIVE-COARSE", 215}
};

spirv::Op Emitter::getProjectiveOp(spirv::Op baseOp, bool isProjective) {
    if (!isProjective) return baseOp;
    switch (baseOp) {
        case spirv::OpImageSampleImplicitLod: return spirv::OpImageSampleProjImplicitLod;
        case spirv::OpImageSampleExplicitLod: return spirv::OpImageSampleProjExplicitLod;
        case spirv::OpImageSampleDrefImplicitLod: return spirv::OpImageSampleProjDrefImplicitLod;
        case spirv::OpImageSampleDrefExplicitLod: return spirv::OpImageSampleProjDrefExplicitLod;
        default: return baseOp;
    }
}

std::vector<uint32_t> Emitter::emit(const ProgramNode& program, const std::set<spirv::Capability>& capabilities) {
    nextId = 1;
    variableIds.clear();
    variableTypeIds.clear();
    variableDataTypes.clear();
    idStorageClasses.clear();
    typeToDataType.clear();
    baseTypeIds.clear();
    typeCache.clear();
    structTypeCache.clear();
    constantIds.clear();
    imageTypeCache.clear();
    sampledImageTypeCache.clear();
    imageOrgs.clear();
    samplerTypeId = 0;
    pushConstantId = 0;
    pushConstantTypeId = 0;
    pushConstantSelectName = "";
    pushConstantMembers.clear();
    uniformBlocks.clear();
    timesCounterIds.clear();
    imageMetadata.clear();
    blockDecs.clear();
    usesImageAtomics = false;
    currentCapabilities.clear();

    capabilityStream.clear();
    extensionStream.clear();
    importStream.clear();
    memoryModelStream.clear();
    entryStream.clear();
    debugStream.clear();
    annoStream.clear();
    typeStream.clear();
    bodyStream.clear();
    entryPointInterfaces.clear();

    currentStream = &capabilityStream;
    currentCapabilities = capabilities;
    shaderStage = program.shaderStage;
    
    std::map<std::string, FDMeta> fdMetas;

    // Phase 0: Extract Environment and File Info
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::ENVIRONMENT) {
            for (const auto& section : div->sections) {
                for (const auto& child : section->children) {
                    if (auto specNames = dynamic_cast<const SpecialNamesNode*>(child.get())) {
                        localSize = specNames->localSize;
                        for (const auto& [hw, cob] : specNames->builtInMappings) builtInMappings[cob] = hw;
                    }
                }
            }
        } else if (div->divisionType == TokenType::DATA) {
            for (const auto& section : div->sections) {
                if (section->getSectionType() == SectionType::FILE_SECTION) {
                    for (const auto& child : section->children) {
                        if (auto fd = dynamic_cast<const FileDescriptionNode*>(child.get())) {
                            FDMeta meta = {BaseType::FLOAT, fd->format, fd->isCoherent, fd->isVolatile};
                            if (!fd->records.empty()) meta.sampledType = fd->records[0]->dataType.baseType;
                            fdMetas[fd->name] = meta;
                        }
                    }
                }
            }
        }
    }

    scanForLoops(program);

    // Phase 1: Emit Sections
    mainFuncId = allocateId();
    extInstSetId = allocateId();
    emitOpString(spirv::OpExtInstImport, extInstSetId, "GLSL.std.450");

    emitAnnotationsWithAccess(program);
    emitInputOutputWithAdvancedTypes(program, fdMetas);
    emitEntryPoints(program);
    emitExecutionModes(program);
    emitTypesAndConstants(program);

    // Pre-declare common GLSL structs
    getOrCreateStructType({getOrCreateBaseType(BaseType::FLOAT), getOrCreateBaseType(BaseType::INT)}); // Frexp
    getOrCreateStructType({getOrCreateBaseType(BaseType::FLOAT), getOrCreateBaseType(BaseType::FLOAT)}); // Modf

    currentStream = &bodyStream;
    emitFunctions(program);
    currentStream = &capabilityStream; // Just in case

    // Capture capabilities and other headers
    emitCapabilities();
    emitMemoryModel();

    // Phase 2: Finalize Assembly
    std::vector<uint32_t> finalSpirv;
    finalSpirv.push_back(spv::MagicNumber);
    finalSpirv.push_back(0x00010300); // SPIR-V 1.3 (Required for GroupNonUniform)
    finalSpirv.push_back(0); // Generator
    finalSpirv.push_back(nextId); // Bound
    finalSpirv.push_back(0); // Reserved

    finalSpirv.insert(finalSpirv.end(), capabilityStream.begin(), capabilityStream.end());
    finalSpirv.insert(finalSpirv.end(), extensionStream.begin(), extensionStream.end());
    finalSpirv.insert(finalSpirv.end(), importStream.begin(), importStream.end());
    finalSpirv.insert(finalSpirv.end(), memoryModelStream.begin(), memoryModelStream.end());
    finalSpirv.insert(finalSpirv.end(), entryStream.begin(), entryStream.end());
    finalSpirv.insert(finalSpirv.end(), debugStream.begin(), debugStream.end());
    finalSpirv.insert(finalSpirv.end(), annoStream.begin(), annoStream.end());
    finalSpirv.insert(finalSpirv.end(), typeStream.begin(), typeStream.end());
    finalSpirv.insert(finalSpirv.end(), bodyStream.begin(), bodyStream.end());
    
    return finalSpirv;
}

void Emitter::emitOp(spirv::Op op, const std::vector<uint32_t>& operands) {
    std::vector<uint32_t>* target = currentStream ? currentStream : &capabilityStream;
    uint32_t opInt = (uint32_t)op;

    switch (op) {
        case spirv::OpCapability:
            target = &capabilityStream;
            break;
        case spirv::OpExtension:
            target = &extensionStream;
            break;
        case spirv::OpExtInstImport:
            target = &importStream;
            break;
        case spirv::OpMemoryModel:
            target = &memoryModelStream;
            break;
        case spirv::OpEntryPoint:
        case spirv::OpExecutionMode:
            target = &entryStream;
            break;
        case spirv::OpString:
        case spirv::OpSource:
        case spirv::OpName:
        case spirv::OpMemberName:
            target = &debugStream;
            break;
        case spirv::OpDecorate:
        case spirv::OpMemberDecorate:
            target = &annoStream;
            break;
        case spirv::OpTypeVoid:
        case spirv::OpTypeBool:
        case spirv::OpTypeInt:
        case spirv::OpTypeFloat:
        case spirv::OpTypeVector:
        case spirv::OpTypeMatrix:
        case spirv::OpTypeImage:
        case spirv::OpTypeSampledImage:
        case spirv::OpTypeSampler:
        case spirv::OpTypeStruct:
        case spirv::OpTypePointer:
        case spirv::OpTypeFunction:
        case spirv::OpConstant:
        case spirv::OpConstantComposite:
        case spirv::OpConstantTrue:
        case spirv::OpConstantFalse:
            target = &typeStream;
            break;
        case spirv::OpVariable:
            // Route local variables to bodyStream if they use Function storage class
            if (operands.size() >= 3 && operands[2] == (uint32_t)spirv::StorageClassFunction) {
                target = &bodyStream;
            } else {
                target = &typeStream;
            }
            break;
        default:
            if (currentStream == &bodyStream) target = &bodyStream;
            else target = &typeStream;
            break;
    }

    uint16_t wordCount = (uint16_t)operands.size() + 1;
    target->push_back(((uint32_t)wordCount << 16) | opInt);
    for (uint32_t operand : operands) {
        target->push_back(operand);
    }
}

void Emitter::emitOpString(spirv::Op op, uint32_t id, const std::string& str) {
    std::vector<uint32_t> operands;
    operands.push_back(id);
    const char* cstr = str.c_str();
    size_t len = str.length() + 1;
    for (size_t i = 0; i < len; i += 4) {
        uint32_t word = 0;
        std::memcpy(&word, cstr + i, std::min((size_t)4, len - i));
        operands.push_back(word);
    }
    emitOp(op, operands);
}

void Emitter::emitCapabilities() {
    emitOp(spirv::OpCapability, {(uint32_t)spirv::CapabilityShader});
    for (auto cap : currentCapabilities) {
        emitOp(spirv::OpCapability, {(uint32_t)cap});
    }
    if (usesImageAtomics) {
        // No special capability needed for OpImageTexelPointer on standard images?
        // Or maybe CapabilityImageQuery? We'll see.
    }
}

void Emitter::emitMemoryModel() {
    emitOp(spirv::OpMemoryModel, {
        (uint32_t)spirv::AddressingModelLogical,
        (uint32_t)spirv::MemoryModelGLSL450
    });
}

void Emitter::emitEntryPoints(const ProgramNode& program) {
    std::vector<uint32_t> operands;
    spirv::ExecutionModel em = spirv::ExecutionModelGLCompute;
    if (shaderStage == ShaderStage::VERTEX) em = spirv::ExecutionModelVertex;
    else if (shaderStage == ShaderStage::FRAGMENT) em = spirv::ExecutionModelFragment;
    
    operands.push_back((uint32_t)em);
    operands.push_back(mainFuncId);
    
    std::string name = "MAIN";
    size_t len = name.length() + 1;
    size_t stringWords = (len + 3) / 4;
    size_t start = operands.size();
    operands.resize(start + stringWords, 0);
    std::memcpy(&operands[start], name.c_str(), name.length());
    
    // Add interfaces (SPIR-V 1.0 requires Input/Output variables)
    for (uint32_t id : entryPointInterfaces) {
        operands.push_back(id);
    }
    
    emitOp(spirv::OpEntryPoint, operands);
}

void Emitter::emitExecutionModes(const ProgramNode& program) {
    if (shaderStage == ShaderStage::COMPUTE) {
        emitOp(spirv::OpExecutionMode, {mainFuncId, 17, (uint32_t)localSize.x, (uint32_t)localSize.y, (uint32_t)localSize.z}); // LocalSize
    } else if (shaderStage == ShaderStage::FRAGMENT) {
        emitOp(spirv::OpExecutionMode, {mainFuncId, (uint32_t)spirv::ExecutionModeOriginUpperLeft});
    }
}

void Emitter::emitAnnotationsWithAccess(const ProgramNode& program) {
    currentStream = &annoStream;
    // 1. Built-ins and Data Items
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::DATA) {
            for (const auto& section : div->sections) {
                for (const auto& node : section->children) {
                    if (auto dataItem = dynamic_cast<DataItemNode*>(node.get())) {
                        if (dataItem->isBuiltIn || dataItem->location != -1) {
                            uint32_t varId = allocateId();
                            variableIds[dataItem->name] = varId;
                            
                            if (dataItem->isBuiltIn) {
                                if (builtInMappings.count(dataItem->name)) {
                                    std::string hw = builtInMappings[dataItem->name];
                                    if (builtInTable.count(hw)) {
                                        const auto& info = builtInTable.at(hw);
                                        if (info.isConstant) continue;
                                        
                                        if (info.stageInterfaces.count(shaderStage) == 0) continue;
                                        
                                        entryPointInterfaces.push_back(varId);
                                        for (auto cap : info.requiredCapabilities) {
                                            currentCapabilities.insert(cap);
                                        }
                                        
                                        emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationBuiltIn, (uint32_t)info.builtInId});
                                        if (shaderStage == ShaderStage::FRAGMENT && 
                                            info.stageInterfaces.at(shaderStage) == spirv::StorageClassInput &&
                                            (dataItem->dataType.baseType == BaseType::INT || dataItem->dataType.baseType == BaseType::UINT)) {
                                            emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationFlat});
                                        }
                                    }
                                }
                            } else if (dataItem->location != -1) {
                                emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationLocation, (uint32_t)dataItem->location});
                                if (dataItem->interpolation != InterpolationMode::NONE) {
                                    if (dataItem->interpolation == InterpolationMode::FLAT)
                                        emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationFlat});
                                    else if (dataItem->interpolation == InterpolationMode::NOPERSPECTIVE)
                                        emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationNoPerspective});
                                } else if (shaderStage == ShaderStage::FRAGMENT && 
                                           (dataItem->dataType.baseType == BaseType::INT || dataItem->dataType.baseType == BaseType::UINT)) {
                                    // Vulkan requires Flat decoration for integer fragment inputs
                                    emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationFlat});
                                }
                                entryPointInterfaces.push_back(varId);
                            }
                        }
                    }
                }
            }
        }
    }

    // 2. IO Resources (SELECT)
    std::regex ioRegex("GPU-(IMAGE|SAMPLER|BUFFER|TEXTURE)-(\\d+)-(\\d+)");
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::INPUT_OUTPUT) {
            for (const auto& section : div->sections) {
                for (const auto& node : section->children) {
                    if (auto sel = dynamic_cast<SelectNode*>(node.get())) {
                        std::smatch match;
                        if (std::regex_search(sel->assignTo, match, ioRegex)) {
                            uint32_t ds = std::stoul(match[2].str());
                            uint32_t bin = std::stoul(match[3].str());
                            uint32_t varId = allocateId();
                            variableIds[sel->name] = varId;
                            // NOTE: prior to SPIRV 1.4, we don't include UniformConstant in OpEntryPoint -- only Input,Output.
                            //entryPointInterfaces.push_back(varId);
                            emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationDescriptorSet, ds});
                            emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationBinding, bin});
                            
                            if (sel->accessMode == AccessMode::READ_ONLY) emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationNonWritable});
                            else if (sel->accessMode == AccessMode::WRITE_ONLY) emitOp(spirv::OpDecorate, {varId, (uint32_t)spirv::DecorationNonReadable});
                        }
                    }
                }
            }
        }
    }
}

void Emitter::emitInputOutput(const ProgramNode& program) {}

void Emitter::emitInputOutputWithAdvancedTypes(const ProgramNode& program, const std::map<std::string, FDMeta>& fdMetas) {
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::INPUT_OUTPUT) {
            for (const auto& section : div->sections) {
                for (const auto& node : section->children) {
                    if (auto sel = dynamic_cast<SelectNode*>(node.get())) {
                        imageOrgs[sel->name] = sel->organization;

                        if (sel->organization == OrganizationType::SAMPLER) {
                            uint32_t varId = lookupVariableId(sel->name);
                            if (samplerTypeId == 0) {
                                samplerTypeId = allocateId();
                                emitOp(spirv::OpTypeSampler, {samplerTypeId});
                            }
                            uint32_t ptrTypeId = getOrCreatePointerType(samplerTypeId, spirv::StorageClassUniformConstant);
                            variableTypeIds[sel->name] = samplerTypeId;
                            emitOp(spirv::OpVariable, {ptrTypeId, varId, (uint32_t)spirv::StorageClassUniformConstant});
                        } else if (sel->organization == OrganizationType::ACCESS_PUSH) {
                            pushConstantSelectName = sel->name;
                        } else if (sel->organization >= OrganizationType::IMAGE_1D && sel->organization <= OrganizationType::STORAGE_CUBE_ARRAY) {
                            uint32_t varId = lookupVariableId(sel->name);
                            BaseType sType = BaseType::FLOAT;
                            spirv::ImageFormat fmt = spirv::ImageFormatUnknown;
                            if (fdMetas.count(sel->name)) {
                                sType = fdMetas.at(sel->name).sampledType;
                                fmt = stringToFormat(fdMetas.at(sel->name).format);
                            }
                            
                            uint32_t sampledId = getOrCreateBaseType(sType);
                            
                            // Map OrganizationType to SPIR-V parameters
                            spirv::Dim dim = spirv::Dim2D;
                            uint32_t arrayed = 0;
                            uint32_t sampledFlag = 1; // 1=Sampled, 2=Storage
                            bool isCombined = false;
                            
                            OrganizationType org = sel->organization;
                            if (org >= OrganizationType::IMAGE_1D && org <= OrganizationType::IMAGE_CUBE_ARRAY) {
                                sampledFlag = 1; isCombined = true;
                            } else if (org >= OrganizationType::TEXTURE_1D && org <= OrganizationType::TEXTURE_CUBE_ARRAY) {
                                sampledFlag = 1; isCombined = false;
                            } else if (org >= OrganizationType::STORAGE_1D && org <= OrganizationType::STORAGE_CUBE_ARRAY) {
                                sampledFlag = 2; isCombined = false;
                            }
                            
                            if (isExtendedFormat(fmt)) {
                                currentCapabilities.insert(spirv::CapabilityStorageImageExtendedFormats);
                            }
                            
                            
                            // Determine dimension and arrayed flag
                            if (org == OrganizationType::IMAGE_1D || org == OrganizationType::TEXTURE_1D || org == OrganizationType::STORAGE_1D) {
                                dim = spirv::Dim1D;
                                currentCapabilities.insert(spirv::CapabilitySampled1D);
                                currentCapabilities.insert(spirv::CapabilityImage1D);
                            } else if (org == OrganizationType::IMAGE_3D || org == OrganizationType::TEXTURE_3D || org == OrganizationType::STORAGE_3D) {
                                dim = spirv::Dim3D;
                            } else if (org == OrganizationType::IMAGE_1D_ARRAY || org == OrganizationType::TEXTURE_1D_ARRAY || org == OrganizationType::STORAGE_1D_ARRAY) {
                                dim = spirv::Dim1D; arrayed = 1;
                                currentCapabilities.insert(spirv::CapabilitySampled1D);
                                currentCapabilities.insert(spirv::CapabilityImage1D);
                            } else if (org == OrganizationType::IMAGE_2D_ARRAY || org == OrganizationType::TEXTURE_2D_ARRAY || org == OrganizationType::STORAGE_2D_ARRAY) {
                                dim = spirv::Dim2D; arrayed = 1;
                            } else if (org == OrganizationType::IMAGE_CUBE || org == OrganizationType::TEXTURE_CUBE || org == OrganizationType::STORAGE_CUBE) {
                                dim = spirv::DimCube;
                            } else if (org == OrganizationType::IMAGE_CUBE_ARRAY || org == OrganizationType::TEXTURE_CUBE_ARRAY || org == OrganizationType::STORAGE_CUBE_ARRAY) {
                                dim = spirv::DimCube; arrayed = 1;
                                currentCapabilities.insert(spirv::CapabilitySampledCubeArray);
                                currentCapabilities.insert(spirv::CapabilityImageCubeArray);
                            }
                            
                            std::string imgKey = std::to_string(sampledId) + "," + std::to_string(sampledFlag) + "," + 
                                               std::to_string((uint32_t)dim) + "," + std::to_string(arrayed) + "," + 
                                               std::to_string((uint32_t)fmt);
                            
                            uint32_t imgTypeId = 0;
                            if (imageTypeCache.count(imgKey)) {
                                imgTypeId = imageTypeCache[imgKey];
                            } else {
                                imgTypeId = allocateId();
                                imageTypeCache[imgKey] = imgTypeId;
                                emitOp(spirv::OpTypeImage, {imgTypeId, sampledId, (uint32_t)dim, 0, arrayed, 0, sampledFlag, (uint32_t)fmt});
                            }
                            
                            imageMetadata[sel->name] = {fmt, sampledFlag, sampledId, imgTypeId, org};
                            
                            uint32_t finalTypeId = imgTypeId;
                            if (isCombined) {
                                finalTypeId = getOrCreateSampledImageType(imgTypeId);
                            }
                            
                            uint32_t ptrTypeId = getOrCreatePointerType(finalTypeId, spirv::StorageClassUniformConstant);
                            variableTypeIds[sel->name] = finalTypeId;
                            emitOp(spirv::OpVariable, {ptrTypeId, varId, (uint32_t)spirv::StorageClassUniformConstant});
                        }
                    }
                }
            }
        }
    }
}

void Emitter::emitTypesAndConstants(const ProgramNode& program) {
    voidTypeId = allocateId();
    emitOp(spirv::OpTypeVoid, {voidTypeId});
    
    voidFuncTypeId = allocateId();
    emitOp(spirv::OpTypeFunction, {voidFuncTypeId, voidTypeId});

    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::DATA) {
            for (const auto& section : div->sections) {
                if (section->getSectionType() == SectionType::FILE_SECTION) continue;
                
                for (const auto& node : section->children) {
                    if (auto dataItem = dynamic_cast<DataItemNode*>(node.get())) {
                        uint32_t typeId = getOrCreateType(dataItem->dataType);
                        spirv::StorageClass sc = spirv::StorageClassPrivate;
                        
                        if (dataItem->isBuiltIn && builtInMappings.count(dataItem->name)) {
                            std::string hw = builtInMappings[dataItem->name];
                            if (builtInTable.count(hw)) {
                                const auto& info = builtInTable.at(hw);
                                if (info.isConstant) {
                                    uint32_t uintType = getOrCreateBaseType(BaseType::UINT);
                                    uint32_t x = getOrCreateConstant(localSize.x, uintType);
                                    uint32_t y = getOrCreateConstant(localSize.y, uintType);
                                    uint32_t z = getOrCreateConstant(localSize.z, uintType);
                                    uint32_t constId = allocateId();
                                    emitOp(spirv::OpConstantComposite, {typeId, constId, x, y, z});
                                    emitOp(spirv::OpDecorate, {constId, (uint32_t)spirv::DecorationBuiltIn, (uint32_t)info.builtInId});
                                    variableIds[dataItem->name] = constId;
                                    variableTypeIds[dataItem->name] = typeId;
                                    variableDataTypes[dataItem->name] = dataItem->dataType;
                                    continue;
                                }
                                
                                if (info.stageInterfaces.count(shaderStage)) {
                                    sc = info.stageInterfaces.at(shaderStage);
                                } else if (!info.stageInterfaces.empty()) {
                                    sc = info.stageInterfaces.begin()->second;
                                }
                            }
                        } else if (dataItem->isInput || section->getSectionType() == SectionType::INPUT_SECTION) {
                            sc = spirv::StorageClassInput;
                        } else if (dataItem->isOutput || section->getSectionType() == SectionType::OUTPUT_SECTION) {
                            sc = spirv::StorageClassOutput;
                        } else if (section->getSectionType() == SectionType::LOCAL_STORAGE) {
                            sc = spirv::StorageClassWorkgroup;
                        }
                        uint32_t varId;
                        if (variableIds.count(dataItem->name)) varId = variableIds[dataItem->name];
                        else { varId = allocateId(); variableIds[dataItem->name] = varId; }

                        // Handle BuiltIn arrays: Vulkan requires these to be arrays
                        if (dataItem->isBuiltIn && builtInMappings.count(dataItem->name)) {
                            std::string hw = builtInMappings[dataItem->name];
                            if (builtInTable.count(hw)) {
                                spirv::BuiltIn bi = builtInTable.at(hw).builtInId;
                                if (bi == spirv::BuiltInSampleMask || bi == spirv::BuiltInClipDistance || bi == spirv::BuiltInCullDistance) {
                                    // If not already an array (or if it's SampleMask which must be size 1), wrap it
                                    if (dataItem->occursCount == 0) {
                                        uint32_t uintType = getOrCreateBaseType(BaseType::UINT);
                                        uint32_t oneId = getOrCreateConstant(1, uintType);
                                        uint32_t arrayTypeId = allocateId();
                                        emitOp(spirv::OpTypeArray, {arrayTypeId, typeId, oneId});
                                        arrayElementTypeIds[arrayTypeId] = typeId;
                                        typeId = arrayTypeId;
                                    }
                                }
                            }
                        }

                        if (dataItem->occursCount > 0) {
                            uint32_t uintType = getOrCreateBaseType(BaseType::UINT);
                            uint32_t countId = getOrCreateConstant(dataItem->occursCount, uintType);
                            uint32_t arrayTypeId = allocateId();
                            emitOp(spirv::OpTypeArray, {arrayTypeId, typeId, countId});
                            
                            arrayElementTypeIds[arrayTypeId] = typeId;
                            typeId = arrayTypeId;
                        }

                        uint32_t ptrTypeId = getOrCreatePointerType(typeId, sc);
                        variableTypeIds[dataItem->name] = typeId;
                        variableDataTypes[dataItem->name] = dataItem->dataType;
                        std::vector<uint32_t> operands = {ptrTypeId, varId, (uint32_t)sc};
                        if (dataItem->initialValue) operands.push_back(emitExpression(*dataItem->initialValue, typeId));
                        emitOp(spirv::OpVariable, operands);
                        idStorageClasses[varId] = sc;
                    }
                }
            }
        }
    }

    // Emit hidden loop counters (Private storage class)
    if (!timesCounterIds.empty()) {
        uint32_t intType = getOrCreateBaseType(BaseType::INT);
        uint32_t ptrType = getOrCreatePointerType(intType, spirv::StorageClassPrivate);
        for (auto const& [node, id] : timesCounterIds) {
            emitOp(spirv::OpVariable, {ptrType, id, (uint32_t)spirv::StorageClassPrivate});
        }
    }

    // Handle Push Constants, Uniform Blocks, and Storage Blocks
    std::string pushConstantSelectName;
    std::vector<std::string> uniformSelectNames;
    std::vector<std::string> storageSelectNames;
    
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::INPUT_OUTPUT) {
            for (const auto& section : div->sections) {
                for (const auto& node : section->children) {
                    if (auto sel = dynamic_cast<SelectNode*>(node.get())) {
                        if (sel->organization == OrganizationType::ACCESS_PUSH) pushConstantSelectName = sel->name;
                        else if (sel->organization == OrganizationType::ACCESS_UNIFORM) uniformSelectNames.push_back(sel->name);
                        else if (sel->organization == OrganizationType::ACCESS_STORAGE) storageSelectNames.push_back(sel->name);
                    }
                }
            }
        }
    }

    // I'll combine the block emission logic into a helper or just refactor.
    auto emitBlock = [&](const std::string& selName, spirv::StorageClass sc, spirv::Decoration blockDec) {
        for (const auto& div : program.divisions) {
            if (div->divisionType == TokenType::DATA) {
                for (const auto& section : div->sections) {
                    if (section->getSectionType() == SectionType::FILE_SECTION) {
                        for (const auto& child : section->children) {
                            if (auto fd = dynamic_cast<FileDescriptionNode*>(child.get())) {
                                if (fd->name == selName) {
                                    std::vector<uint32_t> memberTypeIds;
                                    struct MemberInfo { std::string name; uint32_t typeId; DataType dt; const DataItemNode* node; };
                                    std::vector<MemberInfo> members;
                                    for (const auto& rec : fd->records) {
                                        if (rec->dataType.baseType != BaseType::UNKNOWN) {
                                            uint32_t tid = getOrCreateType(rec->dataType);
                                            
                                            if (rec->occursCount > 0) {
                                                uint32_t uintType = getOrCreateBaseType(BaseType::UINT);
                                                uint32_t countId = getOrCreateConstant(rec->occursCount, uintType);
                                                uint32_t arrayTypeId = allocateId();
                                                emitOp(spirv::OpTypeArray, {arrayTypeId, tid, countId});
                                                
                                                arrayElementTypeIds[arrayTypeId] = tid;
                                                
                                                // Apply ArrayStride to the array type itself
                                                uint32_t strideSize, strideAlign;
                                                LayoutType lt = fd->layout;
                                                if (lt == LayoutType::DEFAULT) {
                                                    if (sc == spirv::StorageClassUniform) lt = LayoutType::STD140;
                                                    else lt = LayoutType::STD430;
                                                }

                                                if (lt == LayoutType::STD140) calculateStd140Layout(rec->dataType, strideSize, strideAlign);
                                                else if (lt == LayoutType::SCALAR) calculateScalarLayout(rec->dataType, strideSize, strideAlign);
                                                else calculateStd430Layout(rec->dataType, strideSize, strideAlign);
                                                
                                                uint32_t stride = strideSize;
                                                if (stride % strideAlign != 0) stride += strideAlign - (stride % strideAlign);
                                                if (lt == LayoutType::STD140 && stride < 16) stride = 16;

                                                emitOp(spirv::OpDecorate, {arrayTypeId, (uint32_t)spirv::DecorationArrayStride, stride});

                                                tid = arrayTypeId;
                                            }

                                            memberTypeIds.push_back(tid);
                                            MemberInfo mi;
                                            mi.name = rec->name;
                                            mi.typeId = tid;
                                            mi.dt = rec->dataType;
                                            mi.node = rec.get();
                                            members.push_back(mi);
                                        }
                                    }

                                    if (!memberTypeIds.empty()) {
                                        uint32_t blockTypeId = allocateId();
                                        std::vector<uint32_t> structOperands = { blockTypeId };
                                        structOperands.insert(structOperands.end(), memberTypeIds.begin(), memberTypeIds.end());
                                        emitOp(spirv::OpTypeStruct, structOperands);
                                        emitOp(spirv::OpDecorate, {blockTypeId, (uint32_t)blockDec});
                                        blockDecs[blockTypeId] = blockDec;

                                        uint32_t offset = 0;
                                        UniformBlockInfo info;
                                        info.typeId = blockTypeId;
                                        for (uint32_t i = 0; i < (uint32_t)memberTypeIds.size(); ++i) {
                                            uint32_t mSize, mAlign;
                                            LayoutType lt = fd->layout;
                                            if (lt == LayoutType::DEFAULT) {
                                                if (sc == spirv::StorageClassUniform) lt = LayoutType::STD140;
                                                else lt = LayoutType::STD430;
                                            }
                                            
                                            if (lt == LayoutType::STD140) calculateStd140Layout(members[i].dt, mSize, mAlign);
                                            else if (lt == LayoutType::SCALAR) calculateScalarLayout(members[i].dt, mSize, mAlign);
                                            else calculateStd430Layout(members[i].dt, mSize, mAlign);
                                            
                                            // In std140, arrays and structs always align to 16 bytes
                                            if (lt == LayoutType::STD140 && (members[i].node->occursCount > 0)) {
                                                if (mAlign < 16) mAlign = 16;
                                            }
                                            
                                            if (offset % mAlign != 0) offset += mAlign - (offset % mAlign);
                                            emitOp(spirv::OpMemberDecorate, {blockTypeId, i, (uint32_t)spirv::DecorationOffset, offset});

                                            // Matrix members need ColMajor + MatrixStride
                                            if (members[i].dt.matrixRows > 0) {
                                                emitOp(spirv::OpMemberDecorate, {blockTypeId, i, (uint32_t)spirv::DecorationColMajor});
                                                // MatrixStride = size of one column vector (rows * 4 bytes), >= 16 in std140
                                                uint32_t colStride = (uint32_t)members[i].dt.matrixRows * 4;
                                                if (lt == LayoutType::STD140 && colStride < 16) colStride = 16;
                                                emitOp(spirv::OpMemberDecorate, {blockTypeId, i, (uint32_t)spirv::DecorationMatrixStride, colStride});
                                            }
                                            
                                            info.members[members[i].name] = {i, members[i].typeId, members[i].node};
                                            
                                            if (members[i].node->occursCount > 0) {
                                                uint32_t strideSize, strideAlign;
                                                if (lt == LayoutType::STD140) calculateStd140Layout(members[i].dt, strideSize, strideAlign);
                                                else if (lt == LayoutType::SCALAR) calculateScalarLayout(members[i].dt, strideSize, strideAlign);
                                                else calculateStd430Layout(members[i].dt, strideSize, strideAlign);
                                                
                                                uint32_t stride = strideSize;
                                                if (stride % strideAlign != 0) stride += strideAlign - (stride % strideAlign);
                                                if (lt == LayoutType::STD140 && stride < 16) stride = 16;
                                                
                                                offset += stride * members[i].node->occursCount;
                                            } else {
                                                offset += mSize;
                                            }
                                        }

                                        uint32_t varId;
                                        if (sc == spirv::StorageClassPushConstant) {
                                            pushConstantId = allocateId(); varId = pushConstantId;
                                            pushConstantTypeId = blockTypeId;
                                            for(auto const& [k, v] : info.members) pushConstantMembers[k] = v;
                                        } else {
                                            varId = lookupVariableId(selName);
                                            info.varId = varId;
                                            uniformBlocks[selName] = info;
                                        }
                                        
                                        uint32_t ptrId = getOrCreatePointerType(blockTypeId, sc);
                                        emitOp(spirv::OpVariable, {ptrId, varId, (uint32_t)sc});
                                        idStorageClasses[varId] = sc;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    };

    if (!pushConstantSelectName.empty()) emitBlock(pushConstantSelectName, spirv::StorageClassPushConstant, spirv::DecorationBlock);
    for (const auto& name : uniformSelectNames) emitBlock(name, spirv::StorageClassUniform, spirv::DecorationBlock);
    for (const auto& name : storageSelectNames) emitBlock(name, spirv::StorageClassUniform, spirv::DecorationBufferBlock);
}

uint32_t Emitter::getOrCreateBaseType(BaseType type) {
    if (baseTypeIds.count(type)) return baseTypeIds[type];
    uint32_t id = allocateId();
    baseTypeIds[type] = id;
    switch (type) {
        case BaseType::FLOAT: emitOp(spirv::OpTypeFloat, {id, 32}); break;
        case BaseType::INT: emitOp(spirv::OpTypeInt, {id, 32, 1}); break;
        case BaseType::UINT: emitOp(spirv::OpTypeInt, {id, 32, 0}); break;
        case BaseType::BOOL: emitOp(spirv::OpTypeBool, {id}); break;
        default: break;
    }
    typeToDataType[id] = {type, 0, 0, 0};
    return id;
}

uint32_t Emitter::getOrCreateType(const DataType& type) {
    std::string key = std::to_string((int)type.baseType) + "," + std::to_string(type.vectorSize) + "," + std::to_string(type.matrixRows) + "," + std::to_string(type.matrixCols);
    if (typeCache.count(key)) return typeCache[key];
    uint32_t baseId = getOrCreateBaseType(type.baseType);
    // Note: resultId might be baseId if it's a scalar.
    // I'll populate the map after I have the final resultId.
    uint32_t resultId = baseId;

    if (type.vectorSize > 0) {
        resultId = allocateId();
        emitOp(spirv::OpTypeVector, {resultId, baseId, (uint32_t)type.vectorSize});
    } else if (type.matrixRows > 0) {
        // Reuse the existing vector type for the column — don't emit a fresh OpTypeVector.
        DataType colVecType = {type.baseType, type.matrixRows, 0, 0};
        uint32_t colVecId = getOrCreateType(colVecType);
        resultId = allocateId();
        emitOp(spirv::OpTypeMatrix, {resultId, colVecId, (uint32_t)type.matrixCols});
    }
    typeCache[key] = resultId;
    typeToDataType[resultId] = type;
    return resultId;
}

uint32_t Emitter::getOrCreateStructType(const std::vector<uint32_t>& memberTypeIds) {
    if (structTypeCache.count(memberTypeIds)) return structTypeCache[memberTypeIds];

    uint32_t id = allocateId();
    std::vector<uint32_t> operands = { id };
    operands.insert(operands.end(), memberTypeIds.begin(), memberTypeIds.end());
    
    // We need to emit this in the types section.
    // If we are currently in a function, this will be invalid.
    // For now, we assume it's called during emitTypesAndConstants.
    emitOp(spirv::OpTypeStruct, operands);
    structTypeCache[memberTypeIds] = id;
    return id;
}

uint32_t Emitter::getOrCreatePointerType(uint32_t typeId, spirv::StorageClass storageClass) {
    std::string key = std::to_string(typeId) + "_" + std::to_string((int)storageClass);
    if (typeCache.count(key)) return typeCache[key];
    uint32_t id = allocateId();
    emitOp(spirv::OpTypePointer, {id, (uint32_t)storageClass, typeId});
    typeCache[key] = id;
    return id;
}

uint32_t Emitter::getOrCreateSampledImageType(uint32_t imageTypeId) {
    if (sampledImageTypeCache.count(imageTypeId)) {
        return sampledImageTypeCache[imageTypeId];
    }
    uint32_t id = allocateId();
    sampledImageTypeCache[imageTypeId] = id;
    emitOp(spirv::OpTypeSampledImage, {id, imageTypeId});
    return id;
}

uint32_t Emitter::getOrCreateConstant(int value, uint32_t typeId) {
    std::string key = std::to_string(value) + "_" + std::to_string(typeId);
    if (constantIds.count(key)) return constantIds[key];
    uint32_t id = allocateId();
    constantIds[key] = id;
    emitOp(spirv::OpConstant, {typeId, id, (uint32_t)value});
    return id;
}

uint32_t Emitter::getOrCreateConstant(const LiteralNode& literal, uint32_t typeId) {
    if (typeId == 0) {
        if (literal.token.lexeme.find('.') != std::string::npos) typeId = getOrCreateBaseType(BaseType::FLOAT);
        else typeId = getOrCreateBaseType(BaseType::INT);
    } else if (typeToDataType.count(typeId) && typeToDataType.at(typeId).vectorSize > 0) {
        typeId = getOrCreateBaseType(typeToDataType.at(typeId).baseType);
    }
    
    std::string key = literal.token.lexeme + "_" + std::to_string(typeId);
    if (constantIds.count(key)) return constantIds[key];
    
    uint32_t id = allocateId();
    constantIds[key] = id;
    
    if (literal.token.type == TokenType::NUMBER) {
        DataType dt = {BaseType::INT, 0, 0, 0};
        if (typeToDataType.count(typeId)) dt = typeToDataType.at(typeId);

        if (dt.baseType == BaseType::FLOAT) {
            float val = std::stof(literal.token.lexeme);
            uint32_t uval; std::memcpy(&uval, &val, 4);
            emitOp(spirv::OpConstant, {typeId, id, uval});
        } else if (dt.baseType == BaseType::UINT) {
            uint32_t val = (uint32_t)std::stoul(literal.token.lexeme);
            emitOp(spirv::OpConstant, {typeId, id, val});
        } else {
            int32_t val = (int32_t)std::stol(literal.token.lexeme);
            emitOp(spirv::OpConstant, {typeId, id, (uint32_t)val});
        }
    }
    return id;
}

void Emitter::scanForLoops(const ProgramNode& program) {
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::PROCEDURE) {
            for (const auto& section : div->sections) {
                std::vector<const StatementNode*> stmts;
                for (const auto& node : section->children) {
                    if (auto stmt = dynamic_cast<const StatementNode*>(node.get())) {
                        if (auto perf = dynamic_cast<const PerformStatementNode*>(stmt)) {
                            if (perf->timesExpr) timesCounterIds.push_back({perf, allocateId()});
                            scanStatements(perf->body);
                        } else if (auto ifStmt = dynamic_cast<const IfStatementNode*>(stmt)) {
                            scanStatements(ifStmt->thenBranch);
                            scanStatements(ifStmt->elseBranch);
                        }
                    }
                }
            }
        }
    }
}

void Emitter::scanStatements(const std::vector<std::unique_ptr<StatementNode>>& statements) {
    for (const auto& stmt : statements) {
        if (auto perf = dynamic_cast<const PerformStatementNode*>(stmt.get())) {
            if (perf->timesExpr) timesCounterIds.push_back({perf, allocateId()});
            scanStatements(perf->body);
        } else if (auto ifStmt = dynamic_cast<const IfStatementNode*>(stmt.get())) {
            scanStatements(ifStmt->thenBranch);
            scanStatements(ifStmt->elseBranch);
        } else if (auto atomic = dynamic_cast<const AtomicOpNode*>(stmt.get())) {
            usesImageAtomics = true;
        }
    }
}

void Emitter::emitFunctions(const ProgramNode& program) {
    inFunctionBody = true;
    emitOp(spirv::OpFunction, {voidTypeId, mainFuncId, 0, voidFuncTypeId});
    uint32_t labelId = allocateId();
    emitOp(spirv::OpLabel, {labelId});
    
    for (const auto& div : program.divisions) {
        if (div->divisionType == TokenType::PROCEDURE) {
            for (const auto& section : div->sections) {
                for (const auto& node : section->children) {
                    if (auto stmt = dynamic_cast<const StatementNode*>(node.get())) emitStatement(*stmt);
                }
            }
        }
    }
    emitOp(spirv::OpFunctionEnd, {});
    inFunctionBody = false;
}

void Emitter::emitStatement(const StatementNode& stmt) {
    if (auto goback = dynamic_cast<const GoBackNode*>(&stmt)) emitOp(spirv::OpReturn, {});
    else if (auto ifStmt = dynamic_cast<const IfStatementNode*>(&stmt)) emitIfStatement(*ifStmt);
    else if (auto perf = dynamic_cast<const PerformStatementNode*>(&stmt)) emitPerformStatement(*perf);
    else if (auto move = dynamic_cast<const MoveNode*>(&stmt)) emitMove(move->source.get(), move->destination.get());
    else if (auto compute = dynamic_cast<const ComputeNode*>(&stmt)) emitMove(compute->expression.get(), compute->destination.get());
    else if (auto read = dynamic_cast<const ReadNode*>(&stmt)) emitRead(read);
    else if (auto write = dynamic_cast<const WriteNode*>(&stmt)) emitWrite(write);
    else if (auto atomic = dynamic_cast<const AtomicOpNode*>(&stmt)) emitAtomicOp(atomic);
    else if (auto call = dynamic_cast<const CallNode*>(&stmt)) emitCall(call);
    else if (auto cohort = dynamic_cast<const CohortOpNode*>(&stmt)) emitCohortOp(cohort);
    else if (auto sync = dynamic_cast<const SyncStatementNode*>(&stmt)) emitSyncStatement(sync);
    else if (auto inquire = dynamic_cast<const InquireStatementNode*>(&stmt)) emitInquireStatement(inquire);
    else if (auto discard = dynamic_cast<const DiscardNode*>(&stmt)) emitOp(spirv::OpKill, {});
    else if (auto interp = dynamic_cast<const InterpolateNode*>(&stmt)) emitInterpolate(interp);

}

void Emitter::emitIfStatement(const IfStatementNode& node) {
    uint32_t condId = emitExpression(*node.condition);
    uint32_t thenLabel = allocateId();
    uint32_t elseLabel = node.elseBranch.empty() ? 0 : allocateId();
    uint32_t mergeLabel = allocateId();

    emitOp(spirv::OpSelectionMerge, {mergeLabel, 0});
    if (elseLabel) {
        emitOp(spirv::OpBranchConditional, {condId, thenLabel, elseLabel});
    } else {
        emitOp(spirv::OpBranchConditional, {condId, thenLabel, mergeLabel});
    }

    emitOp(spirv::OpLabel, {thenLabel});
    for (const auto& stmt : node.thenBranch) {
        emitStatement(*stmt);
    }
    if (node.thenBranch.empty() || 
        (!dynamic_cast<const GoBackNode*>(node.thenBranch.back().get()) && 
         !dynamic_cast<const DiscardNode*>(node.thenBranch.back().get()))) {
        emitOp(spirv::OpBranch, {mergeLabel});
    }

    if (elseLabel) {
        emitOp(spirv::OpLabel, {elseLabel});
        for (const auto& stmt : node.elseBranch) {
            emitStatement(*stmt);
        }
        if (node.elseBranch.empty() || 
            (!dynamic_cast<const GoBackNode*>(node.elseBranch.back().get()) && 
             !dynamic_cast<const DiscardNode*>(node.elseBranch.back().get()))) {
            emitOp(spirv::OpBranch, {mergeLabel});
        }
    }

    emitOp(spirv::OpLabel, {mergeLabel});
}

void Emitter::emitPerformStatement(const PerformStatementNode& node) {
    if (node.timesExpr) {
        uint32_t counterId = 0;
        for (const auto& pair : timesCounterIds) {
            if (pair.first == &node) {
                counterId = pair.second;
                break;
            }
        }
        if (counterId == 0) throw std::runtime_error("Loop counter not found for PERFORM TIMES");
        uint32_t timesId = emitExpression(*node.timesExpr);
        uint32_t intType = getOrCreateBaseType(BaseType::INT);
        uint32_t zeroId = getOrCreateConstant(LiteralNode{Token{TokenType::NUMBER, "0", 0, 0}}, intType);
        emitOp(spirv::OpStore, {counterId, zeroId});

        uint32_t headerLabel = allocateId();
        uint32_t condLabel = allocateId();
        uint32_t bodyLabel = allocateId();
        uint32_t continueLabel = allocateId();
        uint32_t mergeLabel = allocateId();

        emitOp(spirv::OpBranch, {headerLabel});
        emitOp(spirv::OpLabel, {headerLabel});
        emitOp(spirv::OpLoopMerge, {mergeLabel, continueLabel, 0});
        emitOp(spirv::OpBranch, {condLabel});

        emitOp(spirv::OpLabel, {condLabel});
        uint32_t counterValId = allocateId();
        emitOp(spirv::OpLoad, {intType, counterValId, counterId});
        uint32_t condId = allocateId();
        emitOp(spirv::OpSLessThan, {getOrCreateBaseType(BaseType::BOOL), condId, counterValId, timesId});
        emitOp(spirv::OpBranchConditional, {condId, bodyLabel, mergeLabel});

        emitOp(spirv::OpLabel, {bodyLabel});
        for (const auto& stmt : node.body) emitStatement(*stmt);
        emitOp(spirv::OpBranch, {continueLabel});

        emitOp(spirv::OpLabel, {continueLabel});
        uint32_t nextCounterValId = allocateId();
        uint32_t oneId = getOrCreateConstant(LiteralNode{Token{TokenType::NUMBER, "1", 0, 0}}, intType);
        emitOp(spirv::OpIAdd, {intType, nextCounterValId, counterValId, oneId});
        emitOp(spirv::OpStore, {counterId, nextCounterValId});
        emitOp(spirv::OpBranch, {headerLabel});

        emitOp(spirv::OpLabel, {mergeLabel});
        return;
    }

    if (node.varyingVar) {
        emitMove(node.fromExpr.get(), node.varyingVar.get());
    }

    uint32_t headerLabel = allocateId();
    uint32_t condLabel = allocateId();
    uint32_t bodyLabel = allocateId();
    uint32_t continueLabel = allocateId();
    uint32_t mergeLabel = allocateId();

    emitOp(spirv::OpBranch, {headerLabel});
    emitOp(spirv::OpLabel, {headerLabel});
    emitOp(spirv::OpLoopMerge, {mergeLabel, continueLabel, 0});
    emitOp(spirv::OpBranch, {condLabel});

    emitOp(spirv::OpLabel, {condLabel});
    if (node.untilExpr) {
        uint32_t untilId = emitExpression(*node.untilExpr);
        emitOp(spirv::OpBranchConditional, {untilId, mergeLabel, bodyLabel});
    } else {
        emitOp(spirv::OpBranch, {bodyLabel});
    }

    emitOp(spirv::OpLabel, {bodyLabel});
    for (const auto& stmt : node.body) {
        emitStatement(*stmt);
    }
    if (node.body.empty() || 
        (!dynamic_cast<const GoBackNode*>(node.body.back().get()) && 
         !dynamic_cast<const DiscardNode*>(node.body.back().get()))) {
        emitOp(spirv::OpBranch, {continueLabel});
    }

    emitOp(spirv::OpLabel, {continueLabel});
    if (node.varyingVar) {
        uint32_t varId = emitExpression(*node.varyingVar, 0, true);
        uint32_t varValId = emitExpression(*node.varyingVar);
        uint32_t byId = emitExpression(*node.byExpr);
        uint32_t resId = allocateId();
        uint32_t typeId = getExpressionType(*node.varyingVar);
        if (!typeToDataType.count(typeId)) throw std::runtime_error("Internal Error: Missing type metadata for loop variable");
        DataType type = typeToDataType.at(typeId);
        if (type.baseType == BaseType::FLOAT) emitOp(spirv::OpFAdd, {typeId, resId, varValId, byId});
        else emitOp(spirv::OpIAdd, {typeId, resId, varValId, byId});
        emitOp(spirv::OpStore, {varId, resId});
    }
    emitOp(spirv::OpBranch, {headerLabel});

    emitOp(spirv::OpLabel, {mergeLabel});
}

void Emitter::emitMove(const ExpressionNode* sourceExpr, const ExpressionNode* destExpr) {
    uint32_t targetTypeId = getExpressionType(*destExpr);
    uint32_t sourceId = emitExpression(*sourceExpr, targetTypeId);
    
    // Splat scalar to vector if needed
    if (targetTypeId != 0 && typeToDataType.count(targetTypeId)) {
        const auto& targetDT = typeToDataType.at(targetTypeId);
        if (targetDT.vectorSize > 0) {
            uint32_t sourceTypeId = getExpressionType(*sourceExpr);
            if (typeToDataType.count(sourceTypeId)) {
                const auto& sourceDT = typeToDataType.at(sourceTypeId);
                if (sourceDT.vectorSize == 0) {
                    sourceId = splatScalar(sourceId, targetTypeId, targetDT.vectorSize);
                }
            } else {
                // If it's a base type (like FLOAT from a literal)
                sourceId = splatScalar(sourceId, targetTypeId, targetDT.vectorSize);
            }
        }
    }
    
    if (auto identDest = dynamic_cast<const IdentifierNode*>(destExpr)) {
        uint32_t destVarId = emitExpression(*identDest, 0, true);
        emitOp(spirv::OpStore, {destVarId, sourceId});
    } else if (auto qidentDest = dynamic_cast<const QualifiedIdentifierNode*>(destExpr)) {
        uint32_t ptrId = emitExpression(*qidentDest, 0, true);
        emitOp(spirv::OpStore, {ptrId, sourceId});
    } else if (auto swizDest = dynamic_cast<const SwizzleNode*>(destExpr)) {
        uint32_t varId = emitExpression(*swizDest->inner, 0, true);
        uint32_t varTypeId = getExpressionType(*swizDest->inner);
        if (swizDest->components.size() == 1) {
            uint32_t ptrId = emitSwizzle(*swizDest, true);
            emitOp(spirv::OpStore, {ptrId, sourceId});
        } else {
            uint32_t oldValId = allocateId();
            emitOp(spirv::OpLoad, {varTypeId, oldValId, varId});
            uint32_t resId = allocateId();
            std::vector<uint32_t> shuffleOperands = {varTypeId, resId, oldValId, sourceId};
            int varSize = 1;
            if (typeToDataType.count(varTypeId)) {
                varSize = typeToDataType.at(varTypeId).vectorSize;
                if (varSize == 0) varSize = 1;
            }
            for (int i = 0; i < varSize; ++i) {
                int swizIdx = -1;
                for (int j = 0; j < (int)swizDest->components.size(); ++j) { if (swizDest->components[j] == i) { swizIdx = j; break; } }
                if (swizIdx != -1) shuffleOperands.push_back((uint32_t)(varSize + swizIdx));
                else shuffleOperands.push_back((uint32_t)i);
            }
            emitOp(spirv::OpVectorShuffle, shuffleOperands);
            emitOp(spirv::OpStore, {varId, resId});
        }
    }
}

void Emitter::emitRead(const ReadNode* read) {
    uint32_t imgVarId = lookupVariableId(read->imageName);
    uint32_t imgTypeId = 0;
    if (variableTypeIds.count(read->imageName)) imgTypeId = variableTypeIds.at(read->imageName);
    else throw std::runtime_error("Internal Error: Missing type for image: " + read->imageName);

    uint32_t coordId = emitExpression(*read->coordinates);
    uint32_t targetTypeId = getExpressionType(*read->target);
    OrganizationType org = OrganizationType::UNKNOWN;
    if (imageOrgs.count(read->imageName)) org = imageOrgs.at(read->imageName);
    
    uint32_t imgValId = allocateId();
    emitOp(spirv::OpLoad, {imgTypeId, imgValId, imgVarId});
    
    if (imageMetadata.count(read->imageName)) {
        auto& meta = imageMetadata.at(read->imageName);
        if (meta.sampledFlag == 2 && meta.format == spirv::ImageFormatUnknown) {
            currentCapabilities.insert(spirv::CapabilityStorageImageReadWithoutFormat);
        }
    }
    
    uint32_t sampledValId = 0;
    uint32_t lodId = 0;
    if (read->lod) {
        lodId = emitExpression(*read->lod);
    }

    if (read->isFetch) {
        uint32_t imagePortionId = imgValId;
        if (org >= OrganizationType::IMAGE_1D && org <= OrganizationType::IMAGE_CUBE_ARRAY) {
            imagePortionId = allocateId();
            uint32_t rawImgTypeId = imageMetadata.at(read->imageName).imageTypeId;
            emitOp(spirv::OpImage, {rawImgTypeId, imagePortionId, imgValId});
        }
        
        sampledValId = allocateId();
        if (lodId) {
            emitOp(spirv::OpImageFetch, {targetTypeId, sampledValId, imagePortionId, coordId, (uint32_t)spirv::ImageOperandsLodMask, lodId});
        } else {
            emitOp(spirv::OpImageFetch, {targetTypeId, sampledValId, imagePortionId, coordId});
        }
    } else if (read->isGather) {
        uint32_t combinedId = 0;
        if (org >= OrganizationType::IMAGE_1D && org <= OrganizationType::IMAGE_CUBE_ARRAY) {
            combinedId = imgValId;
        } else if (org >= OrganizationType::TEXTURE_1D && org <= OrganizationType::TEXTURE_CUBE_ARRAY && !read->samplerName.empty()) {
            uint32_t smpVarId = lookupVariableId(read->samplerName);
            uint32_t smpTypeId = variableTypeIds.at(read->samplerName);
            uint32_t smpValId = allocateId();
            emitOp(spirv::OpLoad, {smpTypeId, smpValId, smpVarId});
            
            uint32_t rawImgTypeId = imageMetadata.at(read->imageName).imageTypeId;
            uint32_t combinedTypeId = getOrCreateSampledImageType(rawImgTypeId);
            combinedId = allocateId();
            emitOp(spirv::OpSampledImage, {combinedTypeId, combinedId, imgValId, smpValId});
        } else {
            throw std::runtime_error("GATHER requires a sampled image or a texture with a sampler.");
        }
        
        uint32_t compId = 0;
        if (read->component) {
            compId = emitExpression(*read->component);
        } else {
            uint32_t intType = getOrCreateBaseType(BaseType::INT);
            compId = getOrCreateConstant(0, intType);
        }
        
        sampledValId = allocateId();
        emitOp(spirv::OpImageGather, {targetTypeId, sampledValId, combinedId, coordId, compId});
    } else if (org >= OrganizationType::IMAGE_1D && org <= OrganizationType::IMAGE_CUBE_ARRAY) {
        sampledValId = allocateId();
        if (read->compareValue) {
            uint32_t drefId = emitExpression(*read->compareValue);
            if (!read->lod && shaderStage == ShaderStage::FRAGMENT) {
                if (read->lodBias) {
                    uint32_t biasId = emitExpression(*read->lodBias);
                    emitOp(getProjectiveOp(spirv::OpImageSampleDrefImplicitLod, read->isProjective), {targetTypeId, sampledValId, imgValId, coordId, drefId, (uint32_t)spirv::ImageOperandsBiasMask, biasId});
                } else {
                    emitOp(getProjectiveOp(spirv::OpImageSampleDrefImplicitLod, read->isProjective), {targetTypeId, sampledValId, imgValId, coordId, drefId});
                }
            } else {
                if (!lodId) {
                    Token t = {TokenType::NUMBER, "0.0", 0, 0};
                    lodId = getOrCreateConstant(LiteralNode(t), getOrCreateBaseType(BaseType::FLOAT));
                }
                emitOp(getProjectiveOp(spirv::OpImageSampleDrefExplicitLod, read->isProjective), {targetTypeId, sampledValId, imgValId, coordId, drefId, (uint32_t)spirv::ImageOperandsLodMask, lodId});
            }
        } else if (read->gradX && read->gradY) {
            uint32_t gxId = emitExpression(*read->gradX);
            uint32_t gyId = emitExpression(*read->gradY);
            emitOp(getProjectiveOp(spirv::OpImageSampleExplicitLod, read->isProjective), {targetTypeId, sampledValId, imgValId, coordId, (uint32_t)spirv::ImageOperandsGradMask, gxId, gyId});
        } else if (!read->lod && shaderStage == ShaderStage::FRAGMENT) {
            if (read->lodBias) {
                uint32_t biasId = emitExpression(*read->lodBias);
                emitOp(getProjectiveOp(spirv::OpImageSampleImplicitLod, read->isProjective), {targetTypeId, sampledValId, imgValId, coordId, (uint32_t)spirv::ImageOperandsBiasMask, biasId});
            } else {
                emitOp(getProjectiveOp(spirv::OpImageSampleImplicitLod, read->isProjective), {targetTypeId, sampledValId, imgValId, coordId});
            }
        } else {
            if (!lodId) {
                Token t = {TokenType::NUMBER, "0.0", 0, 0};
                lodId = getOrCreateConstant(LiteralNode(t), getOrCreateBaseType(BaseType::FLOAT));
            }
            emitOp(getProjectiveOp(spirv::OpImageSampleExplicitLod, read->isProjective), {targetTypeId, sampledValId, imgValId, coordId, (uint32_t)spirv::ImageOperandsLodMask, lodId});
        }
    } else if (org >= OrganizationType::TEXTURE_1D && org <= OrganizationType::TEXTURE_CUBE_ARRAY && !read->samplerName.empty()) {
        uint32_t smpVarId = lookupVariableId(read->samplerName);
        uint32_t smpTypeId = 0;
        if (variableTypeIds.count(read->samplerName)) smpTypeId = variableTypeIds.at(read->samplerName);
        else throw std::runtime_error("Internal Error: Missing type for sampler: " + read->samplerName);
        uint32_t smpValId = allocateId();
        emitOp(spirv::OpLoad, {smpTypeId, smpValId, smpVarId});
        
        uint32_t rawImgTypeId = imageMetadata.at(read->imageName).imageTypeId;
        uint32_t combinedTypeId = getOrCreateSampledImageType(rawImgTypeId);
        uint32_t combinedId = allocateId();
        emitOp(spirv::OpSampledImage, {combinedTypeId, combinedId, imgValId, smpValId});
        
        sampledValId = allocateId();
        if (read->compareValue) {
            uint32_t drefId = emitExpression(*read->compareValue);
            if (!read->lod && shaderStage == ShaderStage::FRAGMENT) {
                if (read->lodBias) {
                    uint32_t biasId = emitExpression(*read->lodBias);
                    emitOp(getProjectiveOp(spirv::OpImageSampleDrefImplicitLod, read->isProjective), {targetTypeId, sampledValId, combinedId, coordId, drefId, (uint32_t)spirv::ImageOperandsBiasMask, biasId});
                } else {
                    emitOp(getProjectiveOp(spirv::OpImageSampleDrefImplicitLod, read->isProjective), {targetTypeId, sampledValId, combinedId, coordId, drefId});
                }
            } else {
                if (!lodId) {
                    Token t = {TokenType::NUMBER, "0.0", 0, 0};
                    lodId = getOrCreateConstant(LiteralNode(t), getOrCreateBaseType(BaseType::FLOAT));
                }
                emitOp(getProjectiveOp(spirv::OpImageSampleDrefExplicitLod, read->isProjective), {targetTypeId, sampledValId, combinedId, coordId, drefId, (uint32_t)spirv::ImageOperandsLodMask, lodId});
            }
        } else if (read->gradX && read->gradY) {
            uint32_t gxId = emitExpression(*read->gradX);
            uint32_t gyId = emitExpression(*read->gradY);
            emitOp(getProjectiveOp(spirv::OpImageSampleExplicitLod, read->isProjective), {targetTypeId, sampledValId, combinedId, coordId, (uint32_t)spirv::ImageOperandsGradMask, gxId, gyId});
        } else if (!read->lod && shaderStage == ShaderStage::FRAGMENT) {
            if (read->lodBias) {
                uint32_t biasId = emitExpression(*read->lodBias);
                emitOp(getProjectiveOp(spirv::OpImageSampleImplicitLod, read->isProjective), {targetTypeId, sampledValId, combinedId, coordId, (uint32_t)spirv::ImageOperandsBiasMask, biasId});
            } else {
                emitOp(getProjectiveOp(spirv::OpImageSampleImplicitLod, read->isProjective), {targetTypeId, sampledValId, combinedId, coordId});
            }
        } else {
            if (!lodId) {
                Token t = {TokenType::NUMBER, "0.0", 0, 0};
                lodId = getOrCreateConstant(LiteralNode(t), getOrCreateBaseType(BaseType::FLOAT));
            }
            emitOp(getProjectiveOp(spirv::OpImageSampleExplicitLod, read->isProjective), {targetTypeId, sampledValId, combinedId, coordId, (uint32_t)spirv::ImageOperandsLodMask, lodId});
        }
    }
 else if (org >= OrganizationType::STORAGE_1D && org <= OrganizationType::STORAGE_CUBE_ARRAY) {
        sampledValId = allocateId();
        if (lodId) {
            emitOp(spirv::OpImageRead, {targetTypeId, sampledValId, imgValId, coordId, (uint32_t)spirv::ImageOperandsLodMask, lodId});
        } else {
            emitOp(spirv::OpImageRead, {targetTypeId, sampledValId, imgValId, coordId});
        }
    }
    
    if (sampledValId != 0) {
        if (auto identTarget = dynamic_cast<const IdentifierNode*>(read->target.get())) {
            uint32_t targetVarId = lookupVariableId(identTarget->name);
            emitOp(spirv::OpStore, {targetVarId, sampledValId});
        }
    }
}

void Emitter::emitWrite(const WriteNode* write) {
    uint32_t imgVarId = lookupVariableId(write->imageName);
    uint32_t imgTypeId = lookupVariableTypeId(write->imageName);
    uint32_t coordId = emitExpression(*write->coordinates);
    uint32_t sourceId = emitExpression(*write->source);
    
    if (imageMetadata.count(write->imageName)) {
        auto& meta = imageMetadata.at(write->imageName);
        if (meta.sampledFlag == 2 && meta.format == spirv::ImageFormatUnknown) {
            currentCapabilities.insert(spirv::CapabilityStorageImageWriteWithoutFormat);
        }
    }
    
    uint32_t imgValId = allocateId();
    emitOp(spirv::OpLoad, {imgTypeId, imgValId, imgVarId});
    
    emitOp(spirv::OpImageWrite, {imgValId, coordId, sourceId});
}

void Emitter::emitAtomicOp(const AtomicOpNode* atomic) {
    uint32_t ptrId = 0;
    uint32_t texelTypeId = 0;
    spirv::StorageClass sc = spirv::StorageClassUniform;

    // 1. Get Pointer
    if (atomic->coordinates) {
        // It's an image
        const auto* ident = dynamic_cast<const IdentifierNode*>(atomic->target.get());
        if (!ident) throw std::runtime_error("Atomic operation with coordinates must target an image identifier.");
        
        uint32_t imgVarId = lookupVariableId(ident->name);
        uint32_t imgTypeId = 0;
        if (variableTypeIds.count(ident->name)) imgTypeId = variableTypeIds.at(ident->name);
        else throw std::runtime_error("Internal Error: Missing type for image: " + ident->name);
        
        uint32_t coordId = emitExpression(*atomic->coordinates);
        if (imageMetadata.find(ident->name) == imageMetadata.end()) throw std::runtime_error("Internal Error: Missing metadata for image: " + ident->name);
        const auto& meta = imageMetadata.at(ident->name);
        texelTypeId = meta.pixelTypeId;
        
        uint32_t intType = getOrCreateBaseType(BaseType::INT);
        uint32_t zeroId = getOrCreateConstant(0, intType);
        uint32_t ptrTypeId = getOrCreatePointerType(texelTypeId, spirv::StorageClassImage);
        ptrId = allocateId();
        emitOp(spirv::OpImageTexelPointer, {ptrTypeId, ptrId, imgVarId, coordId, zeroId});
        sc = spirv::StorageClassImage;
        usesImageAtomics = true;
    } else {
        // It's a buffer variable
        ptrId = emitExpression(*atomic->target, 0, true);
        texelTypeId = getExpressionType(*atomic->target);
        if (idStorageClasses.count(ptrId)) sc = idStorageClasses.at(ptrId);
    }

    if (typeToDataType.find(texelTypeId) == typeToDataType.end()) throw std::runtime_error("Internal Error: Missing texel type metadata");
    DataType baseType = typeToDataType.at(texelTypeId);
    uint32_t valId = emitExpression(*atomic->value, texelTypeId);
    
    // 2. Determine Opcode
    spirv::Op op;
    switch (atomic->opType) {
        case AtomicOpType::ADD: op = spirv::OpAtomicIAdd; break;
        case AtomicOpType::SUB: op = spirv::OpAtomicISub; break;
        case AtomicOpType::AND: op = spirv::OpAtomicAnd; break;
        case AtomicOpType::OR:  op = spirv::OpAtomicOr; break;
        case AtomicOpType::XOR: op = spirv::OpAtomicXor; break;
        case AtomicOpType::MIN: op = (baseType.baseType == BaseType::UINT) ? spirv::OpAtomicUMin : spirv::OpAtomicSMin; break;
        case AtomicOpType::MAX: op = (baseType.baseType == BaseType::UINT) ? spirv::OpAtomicUMax : spirv::OpAtomicSMax; break;
        case AtomicOpType::EXCHANGE: op = spirv::OpAtomicExchange; break;
        case AtomicOpType::COMPARE_EXCHANGE: op = spirv::OpAtomicCompareExchange; break;
        default: throw std::runtime_error("Unknown atomic op type");
    }

    uint32_t intType = getOrCreateBaseType(BaseType::INT);
    uint32_t scopeId = getOrCreateConstant(1, intType); // ScopeDevice (1)
    uint32_t semanticsId = getOrCreateConstant(0, intType); // MemorySemanticsNone (0)
    
    uint32_t resId = allocateId();
    if (atomic->opType == AtomicOpType::COMPARE_EXCHANGE) {
        uint32_t compId = emitExpression(*atomic->comparator, texelTypeId);
        emitOp(op, {texelTypeId, resId, ptrId, scopeId, semanticsId, semanticsId, valId, compId});
    } else {
        emitOp(op, {texelTypeId, resId, ptrId, scopeId, semanticsId, valId});
    }

    if (atomic->destination) {
        uint32_t targetPtr = emitExpression(*atomic->destination, 0, true);
        emitOp(spirv::OpStore, {targetPtr, resId});
    }
}

void Emitter::emitCohortOp(const CohortOpNode* cohort) {
    uint32_t intType = getOrCreateBaseType(BaseType::INT);
    uint32_t scopeSubgroup = getOrCreateConstant(3, intType); // Subgroup (3)
    
    uint32_t valId = 0;
    uint32_t valTypeId = 0;
    if (cohort->value) {
        valTypeId = getExpressionType(*cohort->value);
        valId = emitExpression(*cohort->value, valTypeId);
    }

    uint32_t resTypeId = getExpressionType(*cohort->destination);
    uint32_t resId = allocateId();
    
    spirv::Op op = spirv::OpNop;
    
    switch (cohort->opType) {
        case CohortOpType::ELECT:
            op = spirv::OpGroupNonUniformElect;
            emitOp(op, {resTypeId, resId, scopeSubgroup});
            break;
        case CohortOpType::ANY:
            op = spirv::OpGroupNonUniformAny;
            emitOp(op, {resTypeId, resId, scopeSubgroup, valId});
            break;
        case CohortOpType::ALL:
            op = spirv::OpGroupNonUniformAll;
            emitOp(op, {resTypeId, resId, scopeSubgroup, valId});
            break;
        case CohortOpType::BALLOT:
            op = spirv::OpGroupNonUniformBallot;
            emitOp(op, {resTypeId, resId, scopeSubgroup, valId});
            break;
        case CohortOpType::BROADCAST:
            op = spirv::OpGroupNonUniformBroadcast;
            {
                uint32_t sourceId = emitExpression(*cohort->sourceId, intType);
                emitOp(op, {resTypeId, resId, scopeSubgroup, valId, sourceId});
            }
            break;
        case CohortOpType::BROADCAST_FIRST:
            op = spirv::OpGroupNonUniformBroadcastFirst;
            emitOp(op, {resTypeId, resId, scopeSubgroup, valId});
            break;
        case CohortOpType::SUM:
        case CohortOpType::PRODUCT:
        case CohortOpType::MIN:
        case CohortOpType::MAX:
        case CohortOpType::AND:
        case CohortOpType::OR:
        case CohortOpType::XOR:
            {
            if (typeToDataType.find(valTypeId) == typeToDataType.end()) throw std::runtime_error("Internal Error: Missing value type metadata");
            DataType dt = typeToDataType.at(valTypeId);
                uint32_t modeId = 0; // Reduce
                if (cohort->mode == CohortMode::INCLUSIVE_SCAN) modeId = 1;
                else if (cohort->mode == CohortMode::EXCLUSIVE_SCAN) modeId = 2;

                if (cohort->opType == CohortOpType::SUM) {
                    op = (dt.baseType == BaseType::FLOAT) ? spirv::OpGroupNonUniformFAdd : spirv::OpGroupNonUniformIAdd;
                } else if (cohort->opType == CohortOpType::PRODUCT) {
                    op = (dt.baseType == BaseType::FLOAT) ? spirv::OpGroupNonUniformFMul : spirv::OpGroupNonUniformIMul;
                } else if (cohort->opType == CohortOpType::MIN) {
                    if (dt.baseType == BaseType::FLOAT) op = spirv::OpGroupNonUniformFMin;
                    else op = (dt.baseType == BaseType::UINT) ? spirv::OpGroupNonUniformUMin : spirv::OpGroupNonUniformSMin;
                } else if (cohort->opType == CohortOpType::MAX) {
                    if (dt.baseType == BaseType::FLOAT) op = spirv::OpGroupNonUniformFMax;
                    else op = (dt.baseType == BaseType::UINT) ? spirv::OpGroupNonUniformUMax : spirv::OpGroupNonUniformSMax;
                } else if (cohort->opType == CohortOpType::AND) {
                    op = (dt.baseType == BaseType::BOOL) ? spirv::OpGroupNonUniformLogicalAnd : spirv::OpGroupNonUniformBitwiseAnd;
                } else if (cohort->opType == CohortOpType::OR) {
                    op = (dt.baseType == BaseType::BOOL) ? spirv::OpGroupNonUniformLogicalOr : spirv::OpGroupNonUniformBitwiseOr;
                } else if (cohort->opType == CohortOpType::XOR) {
                    op = (dt.baseType == BaseType::BOOL) ? spirv::OpGroupNonUniformLogicalXor : spirv::OpGroupNonUniformBitwiseXor;
                }
                
                emitOp(op, {resTypeId, resId, scopeSubgroup, modeId, valId});
            }
            break;
        default: break;
    }

    uint32_t destPtrId = emitExpression(*cohort->destination, 0, true);
    emitOp(spirv::OpStore, {destPtrId, resId});
}

void Emitter::emitSyncStatement(const SyncStatementNode* sync) {
    uint32_t intType = getOrCreateBaseType(BaseType::INT);
    uint32_t scopeWorkgroup = getOrCreateConstant(2, intType); // Workgroup (2)
    
    // WorkgroupMemory (0x100) | AcquireRelease (0x8) | UniformMemory (0x40) | ImageMemory (0x800)
    uint32_t semantics = 0x100 | 0x8 | 0x40 | 0x800;
    uint32_t semanticsId = getOrCreateConstant(semantics, intType);
    
    emitOp(spirv::OpControlBarrier, {scopeWorkgroup, scopeWorkgroup, semanticsId});
}

void Emitter::emitInquireStatement(const InquireStatementNode* inquire) {
    currentCapabilities.insert(spirv::CapabilityImageQuery);
    uint32_t imgVarId = lookupVariableId(inquire->resourceName);
    uint32_t imgTypeId = lookupVariableTypeId(inquire->resourceName);

    uint32_t imgValId = allocateId();
    emitOp(spirv::OpLoad, {imgTypeId, imgValId, imgVarId});
    
    uint32_t resId = allocateId();
    uint32_t resTypeId = 0; 
    
    if (auto destIdent = dynamic_cast<const IdentifierNode*>(inquire->destination.get())) {
        if (variableTypeIds.count(destIdent->name)) resTypeId = variableTypeIds.at(destIdent->name);
        else throw std::runtime_error("Internal Error: Missing type for destination: " + destIdent->name);
    }

    if (imageMetadata.count(inquire->resourceName)) {
        auto& meta = imageMetadata.at(inquire->resourceName);
        uint32_t finalImgValId = imgValId;
        
        // If it's a sampled image (combined), we need to extract the base image for queries
        if (meta.sampledFlag == 1) {
            uint32_t imageOnlyId = allocateId();
            // Note: We need the image type ID without the 'Sampled' property.
            emitOp(spirv::OpImage, {meta.imageTypeId, imageOnlyId, imgValId});
            finalImgValId = imageOnlyId;
        }

        if (inquire->queryType == InquireQueryType::SIZE) {
            if (inquire->lod) {
                uint32_t lodId = emitExpression(*inquire->lod);
                emitOp(spirv::OpImageQuerySizeLod, {resTypeId, resId, finalImgValId, lodId});
            } else {
                if (meta.sampledFlag == 2) {
                    emitOp(spirv::OpImageQuerySize, {resTypeId, resId, finalImgValId});
                } else {
                    uint32_t intType = getOrCreateBaseType(BaseType::INT);
                    uint32_t lod0Id = getOrCreateConstant(0, intType);
                    emitOp(spirv::OpImageQuerySizeLod, {resTypeId, resId, finalImgValId, lod0Id});
                }
            }
        } else if (inquire->queryType == InquireQueryType::LEVELS) {
            emitOp(spirv::OpImageQueryLevels, {resTypeId, resId, finalImgValId});
        }
    }

    uint32_t destPtr = emitExpression(*inquire->destination, 0, true);
    emitOp(spirv::OpStore, {destPtr, resId});
}

void Emitter::emitInterpolate(const InterpolateNode* interp) {
    currentCapabilities.insert(spirv::CapabilityInterpolationFunction);
    
    // 1. Get interpolant address (it MUST be an input variable)
    uint32_t interpolantId = emitExpression(*interp->interpolant, 0, true);
    uint32_t interpolantTypeId = getExpressionType(*interp->interpolant);
    
    uint32_t resId = allocateId();
    uint32_t resTypeId = interpolantTypeId;
    
    switch (interp->type) {
        case InterpolateType::CENTROID:
            emitOp(spirv::OpExtInst, {resTypeId, resId, extInstSetId, 76, interpolantId});
            break;
        case InterpolateType::SAMPLE:
            {
                uint32_t intType = getOrCreateBaseType(BaseType::INT);
                uint32_t sampleId = emitExpression(*interp->sampleIndex, intType);
                emitOp(spirv::OpExtInst, {resTypeId, resId, extInstSetId, 77, interpolantId, sampleId});
            }
            break;
        case InterpolateType::OFFSET:
            {
                uint32_t floatType = getOrCreateBaseType(BaseType::FLOAT);
                uint32_t v2Type = getOrCreateType({BaseType::FLOAT, 2, 0, 0});
                uint32_t offsetId = emitExpression(*interp->offset, v2Type);
                emitOp(spirv::OpExtInst, {resTypeId, resId, extInstSetId, 78, interpolantId, offsetId});
            }
            break;
    }
    
    uint32_t destPtr = emitExpression(*interp->destination, 0, true);
    emitOp(spirv::OpStore, {destPtr, resId});
}



uint32_t Emitter::emitExpression(const ExpressionNode& expr, uint32_t expectedTypeId, bool asPointer) {
    if (auto lit = dynamic_cast<const LiteralNode*>(&expr)) return getOrCreateConstant(*lit, expectedTypeId);
    if (auto ident = dynamic_cast<const IdentifierNode*>(&expr)) {
        // Handle unqualified member lookup (fallback to blocks/push constants)
        uint32_t typeId = 0;
        uint32_t varId = 0;
        bool found = false;

        if (variableIds.count(ident->name)) {
            varId = variableIds.at(ident->name);
            typeId = variableTypeIds.at(ident->name);
            found = true;
        } else {
            for (auto const& [blockName, info] : uniformBlocks) {
                if (info.members.count(ident->name)) {
                    auto const& member = info.members.at(ident->name);
                    Token t = {TokenType::NUMBER, std::to_string(member.index), 0, 0};
                    uint32_t indexId = getOrCreateConstant(LiteralNode(t), getOrCreateBaseType(BaseType::INT));
                    spirv::StorageClass sc = idStorageClasses.count(info.varId) ? idStorageClasses.at(info.varId) : spirv::StorageClassUniform;
                    uint32_t ptrTypeId = getOrCreatePointerType(member.typeId, sc);
                    uint32_t memberPtrId = allocateId();
                    emitOp(spirv::OpAccessChain, {ptrTypeId, memberPtrId, info.varId, indexId});
                    idStorageClasses[memberPtrId] = sc;
                    varId = memberPtrId;
                    typeId = member.typeId;
                    found = true;
                    break;
                }
            }
            if (!found && pushConstantMembers.count(ident->name)) {
                auto const& member = pushConstantMembers.at(ident->name);
                Token t = {TokenType::NUMBER, std::to_string(member.index), 0, 0};
                uint32_t indexId = getOrCreateConstant(LiteralNode(t), getOrCreateBaseType(BaseType::INT));
                uint32_t ptrTypeId = getOrCreatePointerType(member.typeId, spirv::StorageClassPushConstant);
                uint32_t memberPtrId = allocateId();
                emitOp(spirv::OpAccessChain, {ptrTypeId, memberPtrId, pushConstantId, indexId});
                idStorageClasses[memberPtrId] = spirv::StorageClassPushConstant;
                varId = memberPtrId;
                typeId = member.typeId;
                found = true;
            }
        }

        if (!found) throw std::runtime_error("Undeclared identifier (in emitter): " + ident->name);

        // Hack for BuiltIn array wrapping (SampleMask, ClipDistance, CullDistance)
        if (builtInMappings.count(ident->name)) {
            std::string hw = builtInMappings[ident->name];
            if (builtInTable.count(hw)) {
                spirv::BuiltIn bi = builtInTable.at(hw).builtInId;
                if ((bi == spirv::BuiltInSampleMask || bi == spirv::BuiltInClipDistance || bi == spirv::BuiltInCullDistance) && 
                    !ident->subscript && arrayElementTypeIds.count(typeId)) {
                    uint32_t intType = getOrCreateBaseType(BaseType::INT);
                    uint32_t zeroId = getOrCreateConstant(0, intType);
                    spirv::StorageClass sc = idStorageClasses[varId];
                    uint32_t elementTypeId = arrayElementTypeIds.at(typeId);
                    uint32_t ptrTypeId = getOrCreatePointerType(elementTypeId, sc);
                    uint32_t elementPtrId = allocateId();
                    emitOp(spirv::OpAccessChain, {ptrTypeId, elementPtrId, varId, zeroId});
                    idStorageClasses[elementPtrId] = sc;
                    varId = elementPtrId;
                    typeId = elementTypeId;
                }
            }
        }

        if (ident->subscript) {
            uint32_t subId = emitExpression(*ident->subscript);
            uint32_t elementTypeId = arrayElementTypeIds[typeId];
            uint32_t ptrTypeId = getOrCreatePointerType(elementTypeId, idStorageClasses[varId]);
            uint32_t elementPtrId = allocateId();
            emitOp(spirv::OpAccessChain, {ptrTypeId, elementPtrId, varId, subId});
            idStorageClasses[elementPtrId] = idStorageClasses[varId];
            varId = elementPtrId;
            typeId = elementTypeId;
        }

        if (asPointer) return varId;
        if (isConstant(varId)) return varId;

        uint32_t resId = allocateId();
        emitOp(spirv::OpLoad, {typeId, resId, varId});
        return resId;
    }
    if (auto qident = dynamic_cast<const QualifiedIdentifierNode*>(&expr)) {
        if (pushConstantMembers.count(qident->name)) {
            auto const& member = pushConstantMembers.at(qident->name);
            Token t = {TokenType::NUMBER, std::to_string(member.index), 0, 0};
            uint32_t memberIndexId = getOrCreateConstant(LiteralNode(t), getOrCreateBaseType(BaseType::INT));
            
            uint32_t typeId = member.typeId;
            uint32_t varId = pushConstantId;
            spirv::StorageClass sc = spirv::StorageClassPushConstant;
            
            std::vector<uint32_t> indices = { memberIndexId };
            uint32_t finalTypeId = typeId;
            
            if (qident->subscript) {
                indices.push_back(emitExpression(*qident->subscript));
                finalTypeId = arrayElementTypeIds.at(typeId);
            }
            
            uint32_t ptrTypeId = getOrCreatePointerType(finalTypeId, sc);
            uint32_t memberPtrId = allocateId();
            std::vector<uint32_t> operands = { ptrTypeId, memberPtrId, varId };
            operands.insert(operands.end(), indices.begin(), indices.end());
            emitOp(spirv::OpAccessChain, operands);
            idStorageClasses[memberPtrId] = sc;
            
            if (asPointer) return memberPtrId;
            uint32_t resId = allocateId();
            emitOp(spirv::OpLoad, {finalTypeId, resId, memberPtrId});
            return resId;
        }
        for (auto const& [blockName, info] : uniformBlocks) {
            if (info.members.count(qident->name)) {
                auto const& member = info.members.at(qident->name);
                Token t = {TokenType::NUMBER, std::to_string(member.index), 0, 0};
                uint32_t memberIndexId = getOrCreateConstant(LiteralNode(t), getOrCreateBaseType(BaseType::INT));
                spirv::StorageClass sc = idStorageClasses.count(info.varId) ? idStorageClasses.at(info.varId) : spirv::StorageClassUniform;

                uint32_t typeId = member.typeId;
                std::vector<uint32_t> indices = { memberIndexId };
                uint32_t finalTypeId = typeId;

                if (qident->subscript) {
                    indices.push_back(emitExpression(*qident->subscript));
                    finalTypeId = arrayElementTypeIds.at(typeId);
                }

                uint32_t ptrTypeId = getOrCreatePointerType(finalTypeId, sc);
                uint32_t memberPtrId = allocateId();
                std::vector<uint32_t> operands = { ptrTypeId, memberPtrId, info.varId };
                operands.insert(operands.end(), indices.begin(), indices.end());
                emitOp(spirv::OpAccessChain, operands);
                idStorageClasses[memberPtrId] = sc;

                if (asPointer) return memberPtrId;
                uint32_t resId = allocateId();
                emitOp(spirv::OpLoad, {finalTypeId, resId, memberPtrId});
                return resId;
            }
        }
    }
    if (auto swiz = dynamic_cast<const SwizzleNode*>(&expr)) return emitSwizzle(*swiz, asPointer);
    if (auto binary = dynamic_cast<const BinaryExpressionNode*>(&expr)) return emitBinaryExpression(*binary, expectedTypeId);
    if (auto unary = dynamic_cast<const UnaryExpressionNode*>(&expr)) return emitUnaryExpression(*unary, expectedTypeId);
    if (auto vec = dynamic_cast<const VectorLiteralNode*>(&expr)) return emitVectorLiteral(*vec, expectedTypeId);
    if (auto conv = dynamic_cast<const ConversionNode*>(&expr)) return emitConversion(*conv);
    return 0;
}

uint32_t Emitter::getExpressionType(const ExpressionNode& expr) {
    if (auto ident = dynamic_cast<const IdentifierNode*>(&expr)) {
        uint32_t typeId = 0;
        if (variableTypeIds.count(ident->name)) typeId = variableTypeIds.at(ident->name);
        else {
            // Check blocks
            for (auto const& [blockName, info] : uniformBlocks) {
                if (info.members.count(ident->name)) { typeId = info.members.at(ident->name).typeId; break; }
            }
            if (typeId == 0 && pushConstantMembers.count(ident->name)) typeId = pushConstantMembers.at(ident->name).typeId;
        }

        if (typeId == 0) throw std::runtime_error("Undeclared identifier (in emitter): " + ident->name);
        
        // Handle BuiltIn array wrapping in type resolution
        if (builtInMappings.count(ident->name)) {
            std::string hw = builtInMappings.at(ident->name);
            if (builtInTable.count(hw)) {
                spirv::BuiltIn bi = builtInTable.at(hw).builtInId;
                if ((bi == spirv::BuiltInSampleMask || bi == spirv::BuiltInClipDistance || bi == spirv::BuiltInCullDistance) && 
                    !ident->subscript && arrayElementTypeIds.count(typeId)) {
                    return arrayElementTypeIds.at(typeId);
                }
            }
        }

        if (ident->subscript) {
            if (arrayElementTypeIds.count(typeId)) return arrayElementTypeIds.at(typeId);
        }
        return typeId;
    }
    if (auto qident = dynamic_cast<const QualifiedIdentifierNode*>(&expr)) {
        uint32_t typeId = 0;
        if (pushConstantMembers.count(qident->name)) typeId = pushConstantMembers.at(qident->name).typeId;
        else {
            for (auto const& [blockName, info] : uniformBlocks) {
                if (info.members.count(qident->name)) { typeId = info.members.at(qident->name).typeId; break; }
            }
        }
        if (typeId == 0) throw std::runtime_error("Undeclared qualified identifier: " + qident->name);
        if (qident->subscript && arrayElementTypeIds.count(typeId)) return arrayElementTypeIds.at(typeId);
        return typeId;
    }
    if (auto swiz = dynamic_cast<const SwizzleNode*>(&expr)) {
        uint32_t innerTypeId = getExpressionType(*swiz->inner);
        if (typeToDataType.count(innerTypeId)) {
            DataType varType = typeToDataType.at(innerTypeId);
            if (swiz->components.size() == 1) return getOrCreateBaseType(varType.baseType);
            DataType subType = varType; subType.vectorSize = (int)swiz->components.size();
            return getOrCreateType(subType);
        }
        if (swiz->components.size() == 1) return getOrCreateBaseType(BaseType::FLOAT);
        DataType subType = {BaseType::FLOAT, (int)swiz->components.size(), 0, 0};
        return getOrCreateType(subType);
    }
    if (auto vec = dynamic_cast<const VectorLiteralNode*>(&expr)) {
        uint32_t baseType = getExpressionType(*vec->elements[0]);
        DataType type = {BaseType::UNKNOWN, (int)vec->elements.size(), 0, 0};
        uint32_t floatBase = getOrCreateBaseType(BaseType::FLOAT);
        if (baseType == floatBase) type.baseType = BaseType::FLOAT;
        else type.baseType = BaseType::INT;
        return getOrCreateType(type);
    }
    if (auto binary = dynamic_cast<const BinaryExpressionNode*>(&expr)) {
        if (isRelational(binary->op) || binary->op == TokenType::AND || binary->op == TokenType::OR) return getOrCreateBaseType(BaseType::BOOL);
        
        if (binary->op == TokenType::STAR) {
            uint32_t leftTypeId = getExpressionType(*binary->left);
            uint32_t rightTypeId = getExpressionType(*binary->right);
            DataType leftDT = typeToDataType.count(leftTypeId) ? typeToDataType.at(leftTypeId) : DataType{BaseType::FLOAT, 0, 0, 0};
            DataType rightDT = typeToDataType.count(rightTypeId) ? typeToDataType.at(rightTypeId) : DataType{BaseType::FLOAT, 0, 0, 0};

            // Matrix-Vector Multiplication
            if (leftDT.matrixRows > 0 && rightDT.matrixRows == 0 && rightDT.vectorSize > 0) {
                return getOrCreateType({leftDT.baseType, leftDT.matrixRows, 0, 0});
            }
            // Vector-Matrix Multiplication
            if (leftDT.matrixRows == 0 && leftDT.vectorSize > 0 && rightDT.matrixRows > 0) {
                return getOrCreateType({leftDT.baseType, rightDT.matrixCols, 0, 0});
            }
            // Matrix-Matrix Multiplication
            if (leftDT.matrixRows > 0 && rightDT.matrixRows > 0) {
                return getOrCreateType({leftDT.baseType, 0, leftDT.matrixRows, rightDT.matrixCols});
            }
        }
        return getExpressionType(*binary->left);
    }
    if (auto lit = dynamic_cast<const LiteralNode*>(&expr)) {
        if (lit->token.lexeme.find('.') != std::string::npos) return getOrCreateBaseType(BaseType::FLOAT);
        return getOrCreateBaseType(BaseType::INT);
    }
    if (auto unary = dynamic_cast<const UnaryExpressionNode*>(&expr)) {
        if (unary->op == TokenType::NOT) return getOrCreateBaseType(BaseType::BOOL);
        return getExpressionType(*unary->expr);
    }
    if (auto conv = dynamic_cast<const ConversionNode*>(&expr)) {
        uint32_t innerTypeId = getExpressionType(*conv->expr);
        if (typeToDataType.count(innerTypeId)) {
            DataType dt = typeToDataType.at(innerTypeId);
            switch (conv->convType) {
                case TokenType::FLOAT_TO_INT: dt.baseType = BaseType::INT; break;
                case TokenType::FLOAT_TO_UINT: dt.baseType = BaseType::UINT; break;
                case TokenType::INT_TO_FLOAT:
                case TokenType::UINT_TO_FLOAT: dt.baseType = BaseType::FLOAT; break;
                case TokenType::INT_TO_UINT: dt.baseType = BaseType::UINT; break;
                case TokenType::UINT_TO_INT: dt.baseType = BaseType::INT; break;
                default: break;
            }
            return getOrCreateType(dt);
        }
    }
    return 0;
}

bool Emitter::isRelational(TokenType op) {
    return op == TokenType::EQUALS || op == TokenType::EQUAL ||
           op == TokenType::NOT_EQUAL_SYM || op == TokenType::NOT_EQUAL ||
           op == TokenType::GREATER_SYM || op == TokenType::GREATER ||
           op == TokenType::LESS_SYM || op == TokenType::LESS ||
           op == TokenType::GREATER_EQUAL_SYM || op == TokenType::GREATER_EQUAL ||
           op == TokenType::LESS_EQUAL_SYM || op == TokenType::LESS_EQUAL;
}

bool Emitter::isBitwise(TokenType op) {
    return op == TokenType::BIT_AND || op == TokenType::BIT_OR || op == TokenType::BIT_XOR ||
           op == TokenType::BIT_LSHIFT || op == TokenType::BIT_RSHIFT;
}

uint32_t Emitter::emitBinaryExpression(const BinaryExpressionNode& binary, uint32_t expectedTypeId) {
    uint32_t leftTypeId = getExpressionType(*binary.left);
    uint32_t rightTypeId = getExpressionType(*binary.right);
    uint32_t resTypeId = expectedTypeId ? expectedTypeId : leftTypeId;
    if (isRelational(binary.op)) resTypeId = getOrCreateBaseType(BaseType::BOOL);
    uint32_t operandTypeId = isRelational(binary.op) ? leftTypeId : resTypeId;
    uint32_t leftId = emitExpression(*binary.left, operandTypeId);
    uint32_t rightId = emitExpression(*binary.right, operandTypeId);
    uint32_t baseFloatId = getOrCreateBaseType(BaseType::FLOAT);
    uint32_t baseIntId = getOrCreateBaseType(BaseType::INT);
    int vectorSize = 0;
    for (auto const& [key, id] : typeCache) { if (id == resTypeId) { size_t pos = key.find(","); if (pos != std::string::npos) { std::string sub = key.substr(pos + 1); vectorSize = std::stoi(sub.substr(0, sub.find(","))); } break; } }
    
    // Check for specialized matrix/vector math
    if (binary.op == TokenType::STAR) {
        DataType leftDT = typeToDataType.count(leftTypeId) ? typeToDataType.at(leftTypeId) : DataType{BaseType::FLOAT, 0, 0, 0};
        DataType rightDT = typeToDataType.count(rightTypeId) ? typeToDataType.at(rightTypeId) : DataType{BaseType::FLOAT, 0, 0, 0};
        
        if (leftDT.matrixRows > 0 || rightDT.matrixRows > 0) {
            spirv::Op mOp = spirv::OpNop;
            if (leftDT.matrixRows > 0 && rightDT.matrixRows > 0) mOp = spirv::OpMatrixTimesMatrix;
            else if (leftDT.matrixRows > 0 && rightDT.vectorSize > 0) mOp = spirv::OpMatrixTimesVector;
            else if (leftDT.vectorSize > 0 && rightDT.matrixRows > 0) mOp = spirv::OpVectorTimesMatrix;
            
            if (mOp != spirv::OpNop) {
                uint32_t resId = allocateId();
                emitOp(mOp, {resTypeId, resId, leftId, rightId});
                return resId;
            }
        }
    }

    if (vectorSize > 0) {
        if (leftTypeId == baseFloatId || leftTypeId == baseIntId) leftId = splatScalar(leftId, resTypeId, vectorSize);
        if (rightTypeId == baseFloatId || rightTypeId == baseIntId) rightId = splatScalar(rightId, resTypeId, vectorSize);
    }
    spirv::Op opCode = spirv::OpNop;
    bool isFloat = (leftTypeId == baseFloatId);
    if (!isFloat) { for (auto const& [key, id] : typeCache) { if (id == leftTypeId && key.find(std::to_string((int)BaseType::FLOAT)) == 0) { isFloat = true; break; } } }
    switch (binary.op) {
        case TokenType::AND: opCode = spirv::OpLogicalAnd; break;
        case TokenType::OR: opCode = spirv::OpLogicalOr; break;
        case TokenType::PLUS: opCode = isFloat ? spirv::OpFAdd : spirv::OpIAdd; break;
        case TokenType::MINUS: opCode = isFloat ? spirv::OpFSub : spirv::OpISub; break;
        case TokenType::STAR: opCode = isFloat ? spirv::OpFMul : spirv::OpIMul; break;
        case TokenType::SLASH: opCode = isFloat ? spirv::OpFDiv : spirv::OpSDiv; break;
        case TokenType::EQUALS:
        case TokenType::EQUAL:
            opCode = isFloat ? spirv::OpFOrdEqual : spirv::OpIEqual; break;
        case TokenType::NOT_EQUAL_SYM:
        case TokenType::NOT_EQUAL:
            opCode = isFloat ? spirv::OpFOrdNotEqual : spirv::OpINotEqual; break;
        case TokenType::GREATER_SYM:
        case TokenType::GREATER:
            opCode = isFloat ? spirv::OpFOrdGreaterThan : spirv::OpSGreaterThan; break;
        case TokenType::LESS_SYM:
        case TokenType::LESS:
            opCode = isFloat ? spirv::OpFOrdLessThan : spirv::OpSLessThan; break;
        case TokenType::GREATER_EQUAL_SYM:
        case TokenType::GREATER_EQUAL:
            opCode = isFloat ? spirv::OpFOrdGreaterThanEqual : spirv::OpSGreaterThanEqual; break;
        case TokenType::LESS_EQUAL_SYM:
        case TokenType::LESS_EQUAL:
            opCode = isFloat ? spirv::OpFOrdLessThanEqual : spirv::OpSLessThanEqual; break;
        case TokenType::BIT_AND: opCode = spirv::OpBitwiseAnd; break;
        case TokenType::BIT_OR: opCode = spirv::OpBitwiseOr; break;
        case TokenType::BIT_XOR: opCode = spirv::OpBitwiseXor; break;
        case TokenType::BIT_LSHIFT: opCode = spirv::OpShiftLeftLogical; break;
        case TokenType::BIT_RSHIFT: {
            BaseType bt = BaseType::INT;
            if (typeToDataType.count(leftTypeId)) bt = typeToDataType.at(leftTypeId).baseType;
            opCode = (bt == BaseType::UINT) ? spirv::OpShiftRightLogical : spirv::OpShiftRightArithmetic;
            break;
        }
        default: break;
    }
    uint32_t resId = allocateId();
    emitOp(opCode, {resTypeId, resId, leftId, rightId});
    return resId;
}

uint32_t Emitter::emitUnaryExpression(const UnaryExpressionNode& unary, uint32_t expectedTypeId) {
    uint32_t innerId = emitExpression(*unary.expr);
    uint32_t resId = allocateId();
    uint32_t resTypeId = expectedTypeId ? expectedTypeId : getOrCreateBaseType(BaseType::BOOL);
    
    spirv::Op opCode = spirv::OpNop;
    if (unary.op == TokenType::PLUS) return innerId;
    if (unary.op == TokenType::MINUS) {
        resTypeId = getExpressionType(*unary.expr);
        DataType dt = typeToDataType.count(resTypeId) ? typeToDataType.at(resTypeId) : DataType{BaseType::FLOAT, 0, 0, 0};
        opCode = (dt.baseType == BaseType::FLOAT) ? spirv::OpFNegate : spirv::OpSNegate;
    } else if (unary.op == TokenType::NOT) {
        opCode = spirv::OpLogicalNot;
    } else if (unary.op == TokenType::BIT_NOT) {
        opCode = spirv::OpNot;
        resTypeId = getExpressionType(unary);
    }
    
    if (opCode != spirv::OpNop) {
        emitOp(opCode, {resTypeId, resId, innerId});
        return resId;
    }
    return innerId;
}

uint32_t Emitter::emitVectorLiteral(const VectorLiteralNode& vec, uint32_t expectedTypeId) {
    if (expectedTypeId == 0) expectedTypeId = getExpressionType(vec);
    uint32_t baseTypeId = 0;
    for (auto const& [key, id] : typeCache) { if (id == expectedTypeId) { size_t pos = key.find(","); BaseType bt = (BaseType)std::stoi(key.substr(0, pos)); baseTypeId = getOrCreateBaseType(bt); break; } }
    std::vector<uint32_t> elementIds;
    bool allConstant = true;
    for (const auto& el : vec.elements) { if (!dynamic_cast<const LiteralNode*>(el.get())) allConstant = false; elementIds.push_back(emitExpression(*el, baseTypeId)); }
    uint32_t resId = allocateId();
    std::vector<uint32_t> operands = {expectedTypeId, resId};
    operands.insert(operands.end(), elementIds.begin(), elementIds.end());
    if (allConstant && !inFunctionBody) emitOp(spirv::OpConstantComposite, operands);
    else emitOp(spirv::OpCompositeConstruct, operands);
    return resId;
}

uint32_t Emitter::splatScalar(uint32_t scalarId, uint32_t vectorTypeId, int size) {
    uint32_t resId = allocateId();
    std::vector<uint32_t> operands = {vectorTypeId, resId};
    for (int i = 0; i < size; ++i) operands.push_back(scalarId);
    emitOp(spirv::OpCompositeConstruct, operands);
    return resId;
}

void Emitter::emitCall(const CallNode* call) {
    std::string name = call->functionName;
    // Handle "GLSL.std.450.SIN" or just "SIN"
    size_t lastDot = name.find_last_of('.');
    if (lastDot != std::string::npos) name = name.substr(lastDot + 1);
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

    if (coreOps.count(name)) {
        uint32_t opcode = coreOps[name];
        if (opcode >= 210 && opcode <= 215) {
            currentCapabilities.insert(spirv::CapabilityDerivativeControl);
        }
        
        uint32_t valId = emitExpression(*call->arguments[0]);
        uint32_t resultTypeId = getExpressionType(*call->arguments[0]);
        if (!call->destinations.empty()) {
            resultTypeId = getExpressionType(*call->destinations[0]);
        }
        
        uint32_t resultId = allocateId();
        emitOp((spirv::Op)opcode, { resultTypeId, resultId, valId });
        
        if (!call->destinations.empty()) {
            uint32_t destPtr = emitExpression(*call->destinations[0], 0, true);
            emitOp(spirv::OpStore, { destPtr, resultId });
        }
        return;
    }

    if (glslOps.find(name) == glslOps.end()) {
        throw std::runtime_error("Unsupported GLSL instruction: " + name);
    }
    uint32_t opcode = glslOps[name];

    std::vector<uint32_t> argIds;
    for (const auto& arg : call->arguments) {
        argIds.push_back(emitExpression(*arg));
    }

    if (call->destinations.size() <= 1) {
        // Single result
        uint32_t resultTypeId = 0;
        if (!call->destinations.empty()) {
            resultTypeId = getExpressionType(*call->destinations[0]);
        } else {
            // No destination? Default to first arg type.
            resultTypeId = getExpressionType(*call->arguments[0]);
        }

        uint32_t resultId = allocateId();
        std::vector<uint32_t> operands = { resultTypeId, resultId, extInstSetId, opcode };
        operands.insert(operands.end(), argIds.begin(), argIds.end());
        emitOp(spirv::OpExtInst, operands);

        if (!call->destinations.empty()) {
            uint32_t destPtr = emitExpression(*call->destinations[0], 0, true);
            emitOp(spirv::OpStore, { destPtr, resultId });
        }
    } else {
        // Multi-result (Destructuring Struct)
        // We need the struct type. This is tricky without a full type system.
        // For Frexp/Modf, we know the types.
        std::vector<uint32_t> memberTypes;
        for (const auto& dest : call->destinations) {
            memberTypes.push_back(getExpressionType(*dest));
        }
        
        uint32_t structTypeId = getOrCreateStructType(memberTypes);

        uint32_t resultId = allocateId();
        std::vector<uint32_t> operands = { structTypeId, resultId, extInstSetId, opcode };
        operands.insert(operands.end(), argIds.begin(), argIds.end());
        emitOp(spirv::OpExtInst, operands);

        for (size_t i = 0; i < call->destinations.size(); ++i) {
            uint32_t memberId = allocateId();
            emitOp(spirv::OpCompositeExtract, { memberTypes[i], memberId, resultId, (uint32_t)i });
            uint32_t destPtr = emitExpression(*call->destinations[i], 0, true);
            emitOp(spirv::OpStore, { destPtr, memberId });
        }
    }
}

uint32_t Emitter::emitSwizzle(const SwizzleNode& swizzle, bool asPointer) {
    uint32_t varId = emitExpression(*swizzle.inner, 0, true);
    uint32_t varTypeId = getExpressionType(*swizzle.inner);
    bool isConst = isConstant(varId);
    
    BaseType baseType = BaseType::FLOAT;
    if (typeToDataType.count(varTypeId)) baseType = typeToDataType.at(varTypeId).baseType;
    uint32_t baseTypeId = getOrCreateBaseType(baseType);
    
    if (asPointer) {
        if (isConst) throw std::runtime_error("Cannot take pointer to a constant swizzle");
        if (swizzle.components.size() == 1) {
            spirv::StorageClass sc = spirv::StorageClassPrivate;
            if (idStorageClasses.count(varId)) sc = idStorageClasses.at(varId);
            
            uint32_t ptrTypeId = getOrCreatePointerType(baseTypeId, sc);
            Token t = {TokenType::NUMBER, std::to_string(swizzle.components[0]), 0, 0};
            uint32_t idxId = getOrCreateConstant(LiteralNode(t), getOrCreateBaseType(BaseType::INT));
            uint32_t resId = allocateId();
            emitOp(spirv::OpAccessChain, {ptrTypeId, resId, varId, idxId});
            idStorageClasses[resId] = sc;
            return resId;
        } else throw std::runtime_error("Cannot take pointer to multi-component swizzle");
    } else {
        uint32_t fullValId;
        if (isConst) {
            fullValId = varId;
        } else {
            fullValId = allocateId();
            emitOp(spirv::OpLoad, {varTypeId, fullValId, varId});
        }
        if (swizzle.components.size() == 1) {
            uint32_t resId = allocateId();
            emitOp(spirv::OpCompositeExtract, {baseTypeId, resId, fullValId, (uint32_t)swizzle.components[0]});
            return resId;
        } else {
            uint32_t resId = allocateId();
            DataType resType = {baseType, (int)swizzle.components.size(), 0, 0};
            uint32_t resTypeId = getOrCreateType(resType);
            std::vector<uint32_t> operands = {resTypeId, resId, fullValId, fullValId};
            for (int idx : swizzle.components) operands.push_back((uint32_t)idx);
            emitOp(spirv::OpVectorShuffle, operands);
            return resId;
        }
    }
}
    
uint32_t Emitter::lookupVariableId(const std::string& name) {
    if (variableIds.count(name)) return variableIds.at(name);
    throw std::runtime_error("Undeclared variable: " + name);
}

uint32_t Emitter::lookupVariableTypeId(const std::string& name) {
    if (variableTypeIds.count(name)) return variableTypeIds.at(name);
    throw std::runtime_error("Undeclared variable type: " + name);
}

void Emitter::calculateStd430Layout(const DataType& type, uint32_t& size, uint32_t& align) {
    uint32_t baseSize = 4; // float, int, uint are all 4 bytes
    
    if (type.matrixRows > 0) {
        // Matrix
        uint32_t colAlign = (type.matrixRows == 2) ? 8 : 16;
        align = colAlign;
        size = colAlign * type.matrixCols;
    } else if (type.vectorSize > 0) {
        // Vector
        if (type.vectorSize == 2) {
            align = 8;
            size = 8;
        } else {
            align = 16;
            size = type.vectorSize * 4;
        }
    } else {
        // Scalar
        align = baseSize;
        size = baseSize;
    }
}

void Emitter::calculateStd140Layout(const DataType& type, uint32_t& size, uint32_t& align) {
    uint32_t baseSize = 4;
    
    if (type.matrixRows > 0) {
        // Matrix in std140: columns are always 16-byte aligned
        align = 16;
        size = 16 * type.matrixCols;
    } else if (type.vectorSize > 0) {
        if (type.vectorSize == 2) {
            align = 8;
            size = 8;
        } else {
            // vec3 and vec4 are both aligned to 16
            align = 16;
            size = (type.vectorSize == 3) ? 12 : 16;
        }
    } else {
        align = baseSize;
        size = baseSize;
    }
}

void Emitter::calculateScalarLayout(const DataType& type, uint32_t& size, uint32_t& align) {
    // SCALAR layout (VK_EXT_scalar_block_layout): all types align to 4 bytes.
    align = 4;
    if (type.matrixRows > 0) {
        size = type.matrixRows * type.matrixCols * 4;
    } else if (type.vectorSize > 0) {
        size = type.vectorSize * 4;
    } else {
        size = 4;
    }
}
    
bool Emitter::isConstant(uint32_t id) {
    return idStorageClasses.find(id) == idStorageClasses.end();
}

uint32_t Emitter::emitConversion(const ConversionNode& conv) {
    uint32_t innerId = emitExpression(*conv.expr);
    uint32_t innerTypeId = getExpressionType(*conv.expr);
    uint32_t resTypeId = 0;
    spirv::Op op = spirv::OpNop;

    DataType innerDT = typeToDataType.at(innerTypeId);
    DataType resDT = innerDT;

    switch (conv.convType) {
        case TokenType::FLOAT_TO_INT:
            resDT.baseType = BaseType::INT;
            op = spirv::OpConvertFToS;
            break;
        case TokenType::FLOAT_TO_UINT:
            resDT.baseType = BaseType::UINT;
            op = spirv::OpConvertFToU;
            break;
        case TokenType::INT_TO_FLOAT:
            resDT.baseType = BaseType::FLOAT;
            op = spirv::OpConvertSToF;
            break;
        case TokenType::UINT_TO_FLOAT:
            resDT.baseType = BaseType::FLOAT;
            op = spirv::OpConvertUToF;
            break;
        case TokenType::INT_TO_UINT:
            resDT.baseType = BaseType::UINT;
            op = spirv::OpBitcast;
            break;
        case TokenType::UINT_TO_INT:
            resDT.baseType = BaseType::INT;
            op = spirv::OpBitcast;
            break;
        default:
            throw std::runtime_error("Unknown conversion type");
    }

    resTypeId = getOrCreateType(resDT);
    uint32_t resId = allocateId();
    emitOp(op, {resTypeId, resId, innerId});
    return resId;
}

} // namespace cobolv
