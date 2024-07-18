#define RenderPassConstantsType joint::DenoiserHistoryFixConstants
#include "unified_root_parameters.hlsli"

#include "bilinear_filter.hlsli"
#include "space_convertions.hlsli"

static const int g_MaxHistoryFixFrameCount = 4;
static const float g_ViewDepthSensitivity = 0.1;

[numthreads(joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z)]
void CsMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Texture2D<float4> albedoAndRoughnessTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_GBufferAlbedoAndRoughness)];
    Texture2D<float> temporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_TemporalAccumulationBuffer)];
    Texture2D<float> viewDepthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_ViewDepthBuffer)];
    Texture2D<float> noisyVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_NoisyVisibilityBuffer)];

    RWTexture2D<float> reprojectedHistoryTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserHistoryFixRc_ReprojectedHistoryTexture)];

    if (!g_PassConstants.IsHistoryFixEnabled)
    {
        return;
    }

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

    const float4 baseViewDepth = viewDepthBuffer[dispatchThreadId.xy];
    const float4 viewDepthSamples = GatherRedManually(viewDepthBuffer, filter, mipIndex);
    const float4 depthDiff = abs(baseViewDepth - viewDepthSamples);

    float4 customDepthWeights = 1.0;
    if (g_PassConstants.IsViewDepthUsedForWeights)
    {
        if (any(depthDiff > g_ViewDepthSensitivity))
        {
            reprojectedHistoryTexture[dispatchThreadId.xy] = noisyVisibilityBuffer.mips[0][dispatchThreadId.xy];
            return;
        }

        uint closestSampleIndex = 0;
        float diff = depthDiff[0];

        [unroll]
        for (uint i = 1; i < 4; ++i)
        {
            if (depthDiff[i] < diff)
            {
                diff = depthDiff[i];
                closestSampleIndex = i;
            }
        }

        float4 customWeights = 0.0;
        customWeights[closestSampleIndex] = 1.0;
    }

    const float4 noisyVisibilityBufferSamples = GatherRedManually(noisyVisibilityBuffer, filter, mipIndex);
    const float filteredVisibilitySample = ApplyBilinearCustomWeights(filter, noisyVisibilityBufferSamples, customDepthWeights); // #TODO: Make custom weight dependency on view depth

    reprojectedHistoryTexture[dispatchThreadId.xy] = filteredVisibilitySample;
}
