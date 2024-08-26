#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum ThreadCount881
    {
        ThreadCount881_X = 8,
        ThreadCount881_Y = 8,
        ThreadCount881_Z = 1,
    };

    static const uint3 g_ThreadPerGroupCount881 = uint3(ThreadCount881_X, ThreadCount881_Y, ThreadCount881_Z);

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
        DebugOutputType_GBufferViewDepthBuffer,
        DebugOutputType_CurrentShadowVisibility,
        DebugOutputType_TemporalAccumulationBuffer,
        DebugOutputType_ReprojectedHistory,
        DebugOutputType_DenoisedShadowVisibilityBuffer,

        DebugOutputType_SigmaTiles,
        DebugOutputType_SigmaSmoothTiles,
    };

    enum MipGenerationFilterType : uint32_t
    {
        MipGenerationFilterType_Min,
        MipGenerationFilterType_Max,
        MipGenerationFilterType_Average,
    };

} // namespace joint
