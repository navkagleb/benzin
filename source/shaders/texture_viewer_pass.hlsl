#include "joint/texture_viewer_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "fullscreen_helper.hlsli"
#include "gbuffer.hlsli"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_ReferenceTexture, joint::TextureViewerResources::ReferenceTexture);
BenzinDeclareRootResource(RWTexture2D<float4>, g_OutDebugTexture, joint::TextureViewerResources::OutDebugTexture);

[numthreads(16, 16, 1)]
void CsMain(uint2 pixelPosition : SV_DispatchThreadID)
{
    if (any(pixelPosition >= g_PassConsts0.TextureResolution))
    {
        return;
    }

    float4 referenceColor = g_ReferenceTexture[pixelPosition];
    referenceColor = (referenceColor - g_PassConsts0.MinColor) / (g_PassConsts0.MaxColor - g_PassConsts0.MinColor);
    referenceColor = saturate(referenceColor);

    const float3 debugColor = g_PassConsts0.ChannelMask.a == 1 ? referenceColor.a : referenceColor.rgb * g_PassConsts0.ChannelMask.rgb;
    g_OutDebugTexture[pixelPosition] = float4(debugColor, 1.0);
}
