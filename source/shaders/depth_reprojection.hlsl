#include "joint/depth_reprojection_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "space_convertions.hlsli"

#if defined(DEPTH_REPROJECTION)

BenzinDeclareRootResource(Texture2D<float>, g_PrevDepth, joint::DepthResprojectionResources::PrevDepth);
BenzinDeclareRootResource(RWTexture2D<float>, g_ReprojectedDepth, joint::DepthResprojectionResources::ReprojectedDepth);

[numthreads(8, 8, 1)]
void CsMain(uint2 dtid : SV_DispatchThreadID)
{
#if 0
    const float2 ndc = UvToNdc((dtid + 0.5) / g_FrameConsts.RenderResolution);
    const float4 clip = float4(ndc, 1.0, 1.0);
    
    float4 prevClip = mul(clip, g_FrameConsts.ClipToPrevClip);
    prevClip /= prevClip.w;
    
    float2 prevUv = ClipToUv(prevClip);
    prevUv = saturate(prevUv);
#endif

    const float2 prevUv = (dtid + 0.5) * g_FrameConsts.InvRenderResolution;
    const float prevDepth = g_PrevDepth.SampleLevel(g_LinearClampSampler, prevUv, 0.0);

    g_ReprojectedDepth[dtid] = prevDepth;
}

#endif // defined(DEPTH_REPROJECTION)

#if defined(DEPTH_REDUCTION)

BenzinDeclareRootResource(Texture2D<float>, g_SourceMip, joint::DepthReductionResources::SourceMip);
BenzinDeclareRootResource(RWTexture2D<float>, g_DestMip, joint::DepthReductionResources::DestMip);

[numthreads(8, 8, 1)]
void CsMain(uint groupIndex : SV_GroupIndex, uint2 dtid : SV_DispatchThreadID)
{
    // Ref: https://www.3dgep.com/learning-directx-12-4/
    // Ref: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli

    // NOTE: Should dispatch for 1/2 resolution of source mip

    const float2 destMipTexelSize = g_PassConsts.m_DestMipTexelSize;
    float minDepth = 0.0;

    if (!g_PassConsts.m_IsSourceWidthOdd && !g_PassConsts.m_IsSourceHeightOdd)
    {
        const float2 uv = (dtid + 0.5) * destMipTexelSize;
        minDepth = g_SourceMip.SampleLevel(g_MinLinearClampSampler, uv, 0.0);
    }
    else if (g_PassConsts.m_IsSourceWidthOdd && !g_PassConsts.m_IsSourceHeightOdd)
    {
        const float2 uv0 = (dtid + float2(0.25, 0.5)) * destMipTexelSize;
        const float2 uv1 = uv0 + float2(0.5, 0.0) * destMipTexelSize;

        minDepth = min(
            g_SourceMip.SampleLevel(g_MinLinearClampSampler, uv0, 0.0),
            g_SourceMip.SampleLevel(g_MinLinearClampSampler, uv1, 0.0));
    }
    else if (!g_PassConsts.m_IsSourceWidthOdd && g_PassConsts.m_IsSourceHeightOdd)
    {
        const float2 uv0 = (dtid + float2(0.5, 0.25)) * destMipTexelSize;
        const float2 uv1 = uv0 + float2(0.0, 0.5) * destMipTexelSize;

        minDepth = min(
            g_SourceMip.SampleLevel(g_MinLinearClampSampler, uv0, 0.0),
            g_SourceMip.SampleLevel(g_MinLinearClampSampler, uv1, 0.0));
    }
    else
    {
        const float2 uv = (dtid + float2(0.25, 0.25)) * destMipTexelSize;
        const float2 uvOffset = 0.5 * destMipTexelSize;

        const float minDepth0 = min(
            g_SourceMip.SampleLevel(g_MinLinearClampSampler, uv, 0.0),
            g_SourceMip.SampleLevel(g_MinLinearClampSampler, uv + float2(uvOffset.x, 0.0), 0.0));
        const float minDepth1 = min(
            g_SourceMip.SampleLevel(g_MinLinearClampSampler, uv + float2(0.0, uvOffset.y), 0.0),
            g_SourceMip.SampleLevel(g_MinLinearClampSampler, uv + float2(uvOffset.x, uvOffset.y), 0.0));

        minDepth = min(minDepth0, minDepth1);
    }

    g_DestMip[dtid] = minDepth;
}

#endif // defined(DEPTH_REDUCTION)
