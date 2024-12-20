#define RenderPassConstantsType joint::FullScreenDebugConstants
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "fullscreen_helper.hlsli"
#include "gbuffer.hlsli"
#include "space_convertions.hlsli"

float GetFloatByIndex(float4 values, uint index)
{
    switch (index)
    {
        case 0: return values.x;
        case 1: return values.y;
        case 2: return values.z;
        case 3: return values.w;
    }

    return g_NaN;
}

float4 PsMain(VsFullScreenTriangleOutput input) : SV_Target
{
    Texture2D<float4> albedoAndRoughnessTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_AlbedoAndRoughnessTexture)];
    Texture2D<float4> emissiveAndMetallicTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_EmissiveAndMetallicTexture)];
    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_WorldNormalTexture)];
    Texture2D<float2> velocityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_VelocityBuffer)];
    Texture2D<float> viewDepthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_ViewDepthBuffer)];
    Texture2D<float> depthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_DepthBuffer)];

    Texture2D<float> noisyPenumbra = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_NoisyPenumbraTexture)];

    PackedGBuffer packedGBuffer;
    packedGBuffer.Color0 = albedoAndRoughnessTexture.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    packedGBuffer.Color1 = emissiveAndMetallicTexture.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    packedGBuffer.Color2 = worldNormalTexture.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    packedGBuffer.Color3 = float4(velocityBuffer.SampleLevel(g_PointClampSampler, input.Uv, 0.0), 0.0, 0.0);

    UnpackedGBuffer gbuffer = UnpackGBuffer(packedGBuffer);

    const float depth = depthBuffer.SampleLevel(g_PointClampSampler, input.Uv, 0);

    // if (depth == 1.0f)
    // {
    //     discard;
    // }

    switch (g_PassConstants.OutputType)
    {
        case joint::DebugOutputType_GBufferAlbedo: return float4(gbuffer.Albedo, 1.0);
        case joint::DebugOutputType_GBufferRoughness: return float4(gbuffer.Roughness.xxx, 1.0);
        case joint::DebugOutputType_GBufferEmissive: return float4(gbuffer.Emissive, 1.0);
        case joint::DebugOutputType_GBufferMetallic: return float4(gbuffer.Metallic.xxx, 1.0);
        case joint::DebugOutputType_GBufferWorldNormal: return float4(gbuffer.WorldNormal, 1.0);
        case joint::DebugOutputType_GBufferViewDepthBuffer:
        {
            const float viewDepth = viewDepthBuffer.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            const float3 viewPos = ReconstructViewPosition(input.Uv, viewDepth, g_FrameConstants.Camera.UvToViewScale, g_FrameConstants.Camera.UvToViewBias);
            return float4(viewPos, 1.0);

            // const float viewDepth = viewDepthBuffer.SampleLevel(g_PointClampSampler, input.Uv, g_PassConstants.ViewDepthMipIndex);
            // return float4((viewDepth + g_PassConstants.MinViewDepth) / g_PassConstants.MaxViewDepth, 0.0, 0.0, 1.0);
        }
    }

    return float4(1.0, 0.0, 1.0, 1.0);
}
