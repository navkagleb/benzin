#include "unified_root_parameters.hlsli"

#include "bilinear_filter.hlsli"
#include "space_convertions.hlsli"

static const int g_MaxHistoryFixFrameCount = 4;

[numthreads(joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z)]
void CsMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Texture2D<float4> albedoAndRoughnessTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_GBufferAlbedoAndRoughness)];
    Texture2D<float> temporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_TemporalAccumulationBuffer)];
    Texture2D<float> noisyVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_NoisyVisibilityBuffer)];

    RWTexture2D<float> reprojectedHistoryTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_ReprojectedHistoryTexture)];

    if (any(dispatchThreadId.xy > g_FrameConstants.RenderResolution))
    {
        return;
    }

    const float accumulatedFrameCount = temporalAccumulationBuffer[dispatchThreadId.xy];
    const float normalizedAccumulatedFrameCount = saturate(accumulatedFrameCount / g_MaxHistoryFixFrameCount);

    if (normalizedAccumulatedFrameCount == 1.0)
    {
        return;
    }

    const float roughness = albedoAndRoughnessTexture[dispatchThreadId.xy].w;
    const uint mipIndex = g_MaxHistoryFixFrameCount * (1.0 - normalizedAccumulatedFrameCount) * roughness;
    const float2 mipSize = g_FrameConstants.RenderResolution / (1u << mipIndex);

    const float2 uv = DispatchThreadIdToUv(dispatchThreadId, g_FrameConstants.InvRenderResolution);
    const BilinearFilter filter = CreateBilinearFilter(uv, mipSize);

    const float4 noisyVisibilityBufferSamples = GatherRedManually(noisyVisibilityBuffer, filter, mipIndex);
    const float filteredVisibilitySample = ApplyBilinearCustomWeights(filter, noisyVisibilityBufferSamples, 1.0); // #TODO: Make custom weight dependency on view depth

    reprojectedHistoryTexture[dispatchThreadId.xy] = filteredVisibilitySample;
}
