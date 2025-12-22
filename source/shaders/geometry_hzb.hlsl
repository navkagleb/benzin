#include "joint/geometry_resources.hpp"
#include "unified_root_parameters.hlsli"

BenzinDeclareRenderPassConsts(joint::GeometryHzbConsts, g_PassConsts);

[numthreads(8, 8, 1)]
void CsMain(uint groupIndex : SV_GroupIndex, uint2 dtid : SV_DispatchThreadID)
{
    // Ref: https://www.3dgep.com/learning-directx-12-4/
    // Ref: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli

    // NOTE: Should dispatch for 1/2 resolution of source mip

    Texture2D<float> sourceMip = BenzinGetRootResource(joint::GeometryHzbRootParam::SourceMip);
    const float2 destMipTexelSize = g_PassConsts.m_DestMipTexelSize;
    float minDepth = 0.0;

    if (!g_PassConsts.m_IsSourceWidthOdd && !g_PassConsts.m_IsSourceHeightOdd)
    {
        const float2 uv = (dtid + 0.5) * destMipTexelSize;
        minDepth = sourceMip.SampleLevel(g_MinLinearClampSampler, uv, 0.0);
    }
    else if (g_PassConsts.m_IsSourceWidthOdd && !g_PassConsts.m_IsSourceHeightOdd)
    {
        const float2 uv0 = (dtid + float2(0.25, 0.5)) * destMipTexelSize;
        const float2 uv1 = uv0 + float2(0.5, 0.0) * destMipTexelSize;

        minDepth = min(
            sourceMip.SampleLevel(g_MinLinearClampSampler, uv0, 0.0),
            sourceMip.SampleLevel(g_MinLinearClampSampler, uv1, 0.0));
    }
    else if (!g_PassConsts.m_IsSourceWidthOdd && g_PassConsts.m_IsSourceHeightOdd)
    {
        const float2 uv0 = (dtid + float2(0.5, 0.25)) * destMipTexelSize;
        const float2 uv1 = uv0 + float2(0.0, 0.5) * destMipTexelSize;

        minDepth = min(
            sourceMip.SampleLevel(g_MinLinearClampSampler, uv0, 0.0),
            sourceMip.SampleLevel(g_MinLinearClampSampler, uv1, 0.0));
    }
    else
    {
        const float2 uv = (dtid + float2(0.25, 0.25)) * destMipTexelSize;
        const float2 uvOffset = 0.5 * destMipTexelSize;

        const float minDepth0 = min(
            sourceMip.SampleLevel(g_MinLinearClampSampler, uv, 0.0),
            sourceMip.SampleLevel(g_MinLinearClampSampler, uv + float2(uvOffset.x, 0.0), 0.0));
        const float minDepth1 = min(
            sourceMip.SampleLevel(g_MinLinearClampSampler, uv + float2(0.0, uvOffset.y), 0.0),
            sourceMip.SampleLevel(g_MinLinearClampSampler, uv + float2(uvOffset.x, uvOffset.y), 0.0));

        minDepth = min(minDepth0, minDepth1);
    }

    RWTexture2D<float> destMip = BenzinGetRootResource(joint::GeometryHzbRootParam::DestMip);
    destMip[dtid] = minDepth;
}
