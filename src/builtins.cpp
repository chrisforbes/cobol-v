#include "builtins.hpp"

namespace cobolv {

const std::map<std::string, BuiltInInfo> builtInTable = {
    {"GPU-GLOBAL-ID", {spirv::BuiltInGlobalInvocationId, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {}, false}},
    {"GPU-LOCAL-ID", {spirv::BuiltInLocalInvocationId, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {}, false}},
    {"GPU-WORKGROUP-ID", {spirv::BuiltInWorkgroupId, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {}, false}},
    {"GPU-NUM-WORKGROUPS", {spirv::BuiltInNumWorkgroups, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {}, false}},
    {"GPU-LOCAL-INDEX", {spirv::BuiltInLocalInvocationIndex, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {}, false}},
    {"GPU-WORKGROUP-SIZE", {spirv::BuiltInWorkgroupSize, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {}, true}},
    {"GPU-SUBGROUP-ID", {spirv::BuiltInSubgroupId, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {spirv::CapabilityGroupNonUniform}, false}},
    {"GPU-SUBGROUP-SIZE", {spirv::BuiltInSubgroupSize, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {spirv::CapabilityGroupNonUniform}, false}},
    {"GPU-SUBGROUP-INVOCATION-ID", {spirv::BuiltInSubgroupLocalInvocationId, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {spirv::CapabilityGroupNonUniform}, false}},
    {"GPU-NUM-SUBGROUPS", {spirv::BuiltInNumSubgroups, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {spirv::CapabilityGroupNonUniform}, false}},
    {"GPU-SUBGROUP-EQ-MASK", {spirv::BuiltInSubgroupEqMask, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {spirv::CapabilityGroupNonUniform, spirv::CapabilityGroupNonUniformBallot}, false}},
    {"GPU-SUBGROUP-GE-MASK", {spirv::BuiltInSubgroupGeMask, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {spirv::CapabilityGroupNonUniform, spirv::CapabilityGroupNonUniformBallot}, false}},
    {"GPU-SUBGROUP-GT-MASK", {spirv::BuiltInSubgroupGtMask, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {spirv::CapabilityGroupNonUniform, spirv::CapabilityGroupNonUniformBallot}, false}},
    {"GPU-SUBGROUP-LE-MASK", {spirv::BuiltInSubgroupLeMask, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {spirv::CapabilityGroupNonUniform, spirv::CapabilityGroupNonUniformBallot}, false}},
    {"GPU-SUBGROUP-LT-MASK", {spirv::BuiltInSubgroupLtMask, {{ShaderStage::COMPUTE, spirv::StorageClassInput}}, {spirv::CapabilityGroupNonUniform, spirv::CapabilityGroupNonUniformBallot}, false}},
    {"GPU-POSITION", {spirv::BuiltInPosition, {{ShaderStage::VERTEX, spirv::StorageClassOutput}}, {}, false}},
    {"GPU-POINT-SIZE", {spirv::BuiltInPointSize, {{ShaderStage::VERTEX, spirv::StorageClassOutput}}, {}, false}},
    {"GPU-VERTEX-INDEX", {spirv::BuiltInVertexIndex, {{ShaderStage::VERTEX, spirv::StorageClassInput}}, {}, false}},
    {"GPU-INSTANCE-INDEX", {spirv::BuiltInInstanceIndex, {{ShaderStage::VERTEX, spirv::StorageClassInput}}, {}, false}},
    {"GPU-DRAW-ID", {spirv::BuiltInDrawIndex, {{ShaderStage::VERTEX, spirv::StorageClassInput}}, {spirv::CapabilityDrawParameters}, false}},
    {"GPU-BASE-VERTEX", {spirv::BuiltInBaseVertex, {{ShaderStage::VERTEX, spirv::StorageClassInput}}, {spirv::CapabilityDrawParameters}, false}},
    {"GPU-BASE-INSTANCE", {spirv::BuiltInBaseInstance, {{ShaderStage::VERTEX, spirv::StorageClassInput}}, {spirv::CapabilityDrawParameters}, false}},
    {"GPU-FRAG-COORD", {spirv::BuiltInFragCoord, {{ShaderStage::FRAGMENT, spirv::StorageClassInput}}, {}, false}},
    {"GPU-FRAG-DEPTH", {spirv::BuiltInFragDepth, {{ShaderStage::FRAGMENT, spirv::StorageClassOutput}}, {}, false}},
    {"GPU-FRONT-FACING", {spirv::BuiltInFrontFacing, {{ShaderStage::FRAGMENT, spirv::StorageClassInput}}, {}, false}},
    {"GPU-POINT-COORD", {spirv::BuiltInPointCoord, {{ShaderStage::FRAGMENT, spirv::StorageClassInput}}, {}, false}},
    {"GPU-SAMPLE-ID", {spirv::BuiltInSampleId, {{ShaderStage::FRAGMENT, spirv::StorageClassInput}}, {spirv::CapabilitySampleRateShading}, false}},
    {"GPU-SAMPLE-POSITION", {spirv::BuiltInSamplePosition, {{ShaderStage::FRAGMENT, spirv::StorageClassInput}}, {spirv::CapabilitySampleRateShading}, false}},
    {"GPU-SAMPLE-MASK-IN", {spirv::BuiltInSampleMask, {{ShaderStage::FRAGMENT, spirv::StorageClassInput}}, {}, false}},
    {"GPU-SAMPLE-MASK", {spirv::BuiltInSampleMask, {{ShaderStage::FRAGMENT, spirv::StorageClassOutput}}, {}, false}},
    {"GPU-CLIP-DISTANCE", {spirv::BuiltInClipDistance, {{ShaderStage::VERTEX, spirv::StorageClassOutput}, {ShaderStage::FRAGMENT, spirv::StorageClassInput}}, {spirv::CapabilityClipDistance}, false}},
    {"GPU-CULL-DISTANCE", {spirv::BuiltInCullDistance, {{ShaderStage::VERTEX, spirv::StorageClassOutput}, {ShaderStage::FRAGMENT, spirv::StorageClassInput}}, {spirv::CapabilityCullDistance}, false}},
    {"GPU-HELPER-INVOCATION", {spirv::BuiltInHelperInvocation, {{ShaderStage::FRAGMENT, spirv::StorageClassInput}}, {}, false}},
};

} // namespace cobolv
