#pragma once

namespace joint
{

    enum ThreadCount881
    {
        ThreadCount881_X = 8,
        ThreadCount881_Y = 8,
        ThreadCount881_Z = 8,
    };

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
    };

} // namespace joint
