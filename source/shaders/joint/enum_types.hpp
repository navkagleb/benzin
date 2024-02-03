#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum DeferredLightingOutputType : uint
    {
        DeferredLightingOutputType_Final,
        DeferredLightingOutputType_ReconsructedWorldPosition,
        DeferredLightingOutputType_GBuffer_Depth,
        DeferredLightingOutputType_GBuffer_Albedo,
        DeferredLightingOutputType_GBuffer_WorldNormal,
        DeferredLightingOutputType_GBuffer_Emissive,
        DeferredLightingOutputType_GBuffer_Roughness,
        DeferredLightingOutputType_GBuffer_Metalness,
        DeferredLightingOutputType_Count,
    };

} // namespace joint
