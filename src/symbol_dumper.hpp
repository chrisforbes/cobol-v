#pragma once

#include "semantics.hpp"
#include <iostream>
#include <iomanip>
#include <map>
#include <string>

namespace cobolv {

class SymbolDumper {
public:
    void dump(const std::map<std::string, Symbol>& symbolTable) {
        std::cout << "\n--- SYMBOL TABLE DUMP ---\n";
        std::cout << std::left 
                  << std::setw(25) << "Name" 
                  << std::setw(15) << "Type" 
                  << std::setw(15) << "Storage" 
                  << std::setw(10) << "Flags" 
                  << std::setw(10) << "Location"
                  << "Extra Info\n";
        std::cout << std::string(85, '-') << "\n";

        for (const auto& [name, symbol] : symbolTable) {
            std::cout << std::left 
                      << std::setw(25) << symbol.name
                      << std::setw(15) << dataTypeToString(symbol.type)
                      << std::setw(15) << storageClassToString(symbol.storageClass)
                      << std::setw(10) << getFlagsString(symbol)
                      << std::setw(10) << (symbol.location == -1 ? "-" : std::to_string(symbol.location));
            
            if (symbol.isResource) {
                std::cout << "Org: " << organizationToString(symbol.resourceInfo.organization)
                          << ", Mode: " << accessModeToString(symbol.resourceInfo.accessMode)
                          << ", Format: " << (symbol.resourceInfo.format.empty() ? "N/A" : symbol.resourceInfo.format);
            } else if (!symbol.hardwareName.empty()) {
                std::cout << "HW: " << symbol.hardwareName;
            }

            if (symbol.occursCount > 0) {
                std::cout << " [OCCURS " << symbol.occursCount << "]";
            }

            std::cout << "\n";
        }
        std::cout << "--- END OF SYMBOL TABLE ---\n\n";
    }

private:
    std::string dataTypeToString(const DataType& dt) {
        std::string s;
        switch (dt.baseType) {
            case BaseType::FLOAT: s = "FLOAT"; break;
            case BaseType::INT: s = "INT"; break;
            case BaseType::UINT: s = "UINT"; break;
            case BaseType::BOOL: s = "BOOL"; break;
            default: s = "UNKNOWN"; break;
        }
        if (dt.vectorSize > 0) s += "V" + std::to_string(dt.vectorSize);
        if (dt.matrixRows > 0) s += "M" + std::to_string(dt.matrixRows) + "x" + std::to_string(dt.matrixCols);
        return s;
    }

    std::string storageClassToString(spirv::StorageClass sc) {
        switch (sc) {
            case spirv::StorageClassUniformConstant: return "UniformConst";
            case spirv::StorageClassInput: return "Input";
            case spirv::StorageClassUniform: return "Uniform";
            case spirv::StorageClassOutput: return "Output";
            case spirv::StorageClassWorkgroup: return "Workgroup";
            case spirv::StorageClassCrossWorkgroup: return "CrossWorkgroup";
            case spirv::StorageClassPrivate: return "Private";
            case spirv::StorageClassFunction: return "Function";
            case spirv::StorageClassGeneric: return "Generic";
            case spirv::StorageClassPushConstant: return "PushConstant";
            case spirv::StorageClassAtomicCounter: return "AtomicCounter";
            case spirv::StorageClassImage: return "Image";
            case spirv::StorageClassStorageBuffer: return "StorageBuffer";
            case spirv::StorageClassMax: return "-";
            default: return "SC(" + std::to_string((int)sc) + ")";
        }
    }

    std::string getFlagsString(const Symbol& s) {
        std::string f;
        if (s.isResource) f += "R";
        if (s.isBuiltIn) f += "B";
        return f.empty() ? "-" : f;
    }

    std::string organizationToString(OrganizationType org) {
        switch (org) {
            case OrganizationType::IMAGE_1D: return "IMG1D";
            case OrganizationType::IMAGE_2D: return "IMG2D";
            case OrganizationType::IMAGE_3D: return "IMG3D";
            case OrganizationType::IMAGE_1D_ARRAY: return "IMG1D_ARR";
            case OrganizationType::IMAGE_2D_ARRAY: return "IMG2D_ARR";
            case OrganizationType::IMAGE_CUBE: return "IMGCUBE";
            case OrganizationType::IMAGE_CUBE_ARRAY: return "IMGCUBE_ARR";
            case OrganizationType::TEXTURE_1D: return "TEX1D";
            case OrganizationType::TEXTURE_2D: return "TEX2D";
            case OrganizationType::TEXTURE_3D: return "TEX3D";
            case OrganizationType::TEXTURE_1D_ARRAY: return "TEX1D_ARR";
            case OrganizationType::TEXTURE_2D_ARRAY: return "TEX2D_ARR";
            case OrganizationType::TEXTURE_CUBE: return "TEXCUBE";
            case OrganizationType::TEXTURE_CUBE_ARRAY: return "TEXCUBE_ARR";
            case OrganizationType::STORAGE_1D: return "STR1D";
            case OrganizationType::STORAGE_2D: return "STR2D";
            case OrganizationType::STORAGE_3D: return "STR3D";
            case OrganizationType::STORAGE_1D_ARRAY: return "STR1D_ARR";
            case OrganizationType::STORAGE_2D_ARRAY: return "STR2D_ARR";
            case OrganizationType::STORAGE_CUBE: return "STRCUBE";
            case OrganizationType::STORAGE_CUBE_ARRAY: return "STRCUBE_ARR";
            case OrganizationType::SAMPLER: return "SAMPLER";
            case OrganizationType::ACCESS_PUSH: return "PUSH";
            case OrganizationType::ACCESS_UNIFORM: return "UNIFORM";
            case OrganizationType::ACCESS_STORAGE: return "STORAGE";
            case OrganizationType::UNKNOWN: return "UNK";
            default: return "ORG(" + std::to_string((int)org) + ")";
        }
    }

    std::string accessModeToString(AccessMode mode) {
        switch (mode) {
            case AccessMode::READ_ONLY: return "READ";
            case AccessMode::WRITE_ONLY: return "WRITE";
            case AccessMode::READ_WRITE: return "RW";
            case AccessMode::UNKNOWN: return "UNK";
            default: return "MODE(" + std::to_string((int)mode) + ")";
        }
    }
};

} // namespace cobolv
