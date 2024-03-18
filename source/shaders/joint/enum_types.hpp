#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum DebugOutputType : uint32_t
    {
        DebugOutputType_None,
        DebugOutputType_ReconsructedWorldPosition,
        DebugOutputType_GBufferAlbedo,
        DebugOutputType_GBufferRoughness,
        DebugOutputType_GBufferEmissive,
        DebugOutputType_GBufferMetallic,
        DebugOutputType_GBufferWorldNormal,
        DebugOutputType_GBufferVelocityBuffer,
        DebugOutputType_CurrentShadowVisibility,
        DebugOutputType_PreviousShadowVisibility,
        DebugOutputType_TemporalAccumulationBuffer,
        DebugOutputType_Count,
    };

} // namespace joint
