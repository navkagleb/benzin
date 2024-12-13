#include "unified_root_parameters.hlsli"
#include "joint/sigma_denoiser_resources.hpp"

BenzinDeclareRootResource(Texture2D<float2>, g_SmoothTiles, joint::Rc_SigmaCopyHistory::SmoothTiles);
BenzinDeclareRootResource(Texture2D<float>, g_ShadowHistory, joint::Rc_SigmaCopyHistory::ShadowHistory);
BenzinDeclareRootResource(Texture2D<uint>, g_HistoryLength, joint::Rc_SigmaCopyHistory::HistoryLength);

BenzinDeclareRootResource(RWTexture2D<float>, g_OutShadowHistory, joint::Rc_SigmaCopyHistory::OutShadowHistory);
BenzinDeclareRootResource(RWTexture2D<uint>, g_OutHistoryLength, joint::Rc_SigmaCopyHistory::OutHistoryLength);

[numthreads(8, 16, 1)]
void CsMain(uint2 pixelPos : SV_DispatchThreadID)
{
    const bool isSky = g_SmoothTiles[pixelPos >> 4].y;
    if (isSky && !g_FrameConstants.IsRenderResolutionChanged)
    {
        return;
    }

    g_OutShadowHistory[pixelPos] = g_ShadowHistory[pixelPos];
    g_OutHistoryLength[pixelPos] = g_HistoryLength[pixelPos];
}
