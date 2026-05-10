#pragma once

#include "ast.hpp"
#include <spirv/unified1/spirv.hpp>
#include <string>
#include <vector>
#include <map>

namespace cobolv {

namespace spirv = spv;

struct BuiltInInfo {
    spirv::BuiltIn builtInId;
    std::map<ShaderStage, spirv::StorageClass> stageInterfaces;
    std::vector<spirv::Capability> requiredCapabilities;
    bool isConstant = false;
};

extern const std::map<std::string, BuiltInInfo> builtInTable;

} // namespace cobolv
