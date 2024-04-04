#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    // ThreadCount
    namespace tc
    {

        enum DenoiserTemporalAccumulation : uint32_t
        {
            DenoiserTemporalAccumulation_X = 8,
            DenoiserTemporalAccumulation_Y = 8,
            DenoiserTemporalAccumulation_Z = 1,
        };

        enum MipGeneration : uint32_t
        {
            MipGeneration_X = 8,
            MipGeneration_Y = 8,
            MipGeneration_Z = 1,
        };

        enum DenoiserHistoryFix : uint32_t
        {
            DenoiserHistoryFix_X = 8,
            DenoiserHistoryFix_Y = 8,
            DenoiserHistoryFix_Z = 1,
        };

    }

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
        DebugOutputType_Count,
    };

    enum EquirectangularToCubeThreadCount : uint32_t
    {
        EquirectangularToCubeThreadCount_X = 8,
        EquirectangularToCubeThreadCount_Y = 8,
        EquirectangularToCubeThreadCount_Z = 1,
    };

} // namespace joint
