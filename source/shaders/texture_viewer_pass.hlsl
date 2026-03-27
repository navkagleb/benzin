#include "joint/texture_viewer_resources.hpp"
#include "unified_root_parameters.hlsli"

BenzinDeclareRenderPassConsts(joint::TextureViewerConsts, g_PassConsts);
BenzinDeclareRootResource(Texture2D<float4>, g_ReferenceTexture, joint::TextureViewerResources::ReferenceTexture);
BenzinDeclareRootResource(RWTexture2D<float4>, g_OutDebugTexture, joint::TextureViewerResources::OutDebugTexture);

[numthreads(16, 16, 1)]
void CsMain(uint2 pixelPosition : SV_DispatchThreadID)
{
    if (any(pixelPosition >= g_PassConsts.TextureResolution))
        return;

    float4 referenceColor = g_ReferenceTexture[pixelPosition];
    referenceColor = (referenceColor - g_PassConsts.MinColor) / (g_PassConsts.MaxColor - g_PassConsts.MinColor);
    referenceColor = saturate(referenceColor);

    const float3 debugColor = g_PassConsts.ChannelMask.a == 1 ? referenceColor.a : referenceColor.rgb * g_PassConsts.ChannelMask.rgb;
    g_OutDebugTexture[pixelPosition] = float4(debugColor, 1.0);
}
