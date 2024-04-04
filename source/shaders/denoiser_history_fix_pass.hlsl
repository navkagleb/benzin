#include "common.hlsli"

#include "bilinear_filter.hlsli"

static const int g_MaxHistoryFixFrameCount = 4;

[numthreads(joint::tc::DenoiserHistoryFix_X, joint::tc::DenoiserHistoryFix_Y, joint::tc::DenoiserHistoryFix_Z)]
void CsMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    ConstantBuffer<joint::FrameConstants> frameConstants = FetchFrameConstantBuffer();

    Texture2D<float4> albedoAndRoughnessTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_GBufferAlbedoAndRoughness)];
    Texture2D<float> temporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_TemporalAccumulationBuffer)];
    Texture2D<float> noisyVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_NoisyVisibilityBuffer)];

    RWTexture2D<float> denoisedVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_DenoisedVisibilityBuffer)];

    if (any(dispatchThreadId.xy > frameConstants.RenderResolution))
    {
        return;
    }

    const float roughness = albedoAndRoughnessTexture[dispatchThreadId.xy].w;

    const float accumulatedFrameCount = temporalAccumulationBuffer[dispatchThreadId.xy];
    const float normalizedAccumulatedFrameCount = saturate(accumulatedFrameCount / g_MaxHistoryFixFrameCount);

    if (normalizedAccumulatedFrameCount == 1.0)
    {
        denoisedVisibilityBuffer[dispatchThreadId.xy] = noisyVisibilityBuffer.mips[0][dispatchThreadId.xy];
        return;
    }

    const uint mipIndex = g_MaxHistoryFixFrameCount * (1.0 - normalizedAccumulatedFrameCount) * roughness;
    const float2 mipSize = frameConstants.RenderResolution / (1u << mipIndex);

    const float2 uv = (dispatchThreadId.xy + 0.5) * frameConstants.InvRenderResolution;
    const BilinearFilter filter = CreateBilinearFilter(uv, mipSize);

    const float4 noisyVisibilityBufferSamples = GatherRedManually(noisyVisibilityBuffer, filter, mipIndex);
    const float filteredVisibilitySample = ApplyBilinearCustomWeights(filter, noisyVisibilityBufferSamples, 1.0);

    denoisedVisibilityBuffer[dispatchThreadId.xy] = filteredVisibilitySample;
}
