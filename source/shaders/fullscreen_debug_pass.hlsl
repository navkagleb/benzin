#define RenderPassConstantsType joint::FullScreenDebugConstants
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "fullscreen_helper.hlsli"
#include "gbuffer.hlsli"
#include "sigma_denoiser/sigma_public.hlsli"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_SigmaTiles, joint::FullScreenDebugRc_SigmaTiles);
BenzinDeclareRootResource(Texture2D<float2>, g_SigmaSmoothTiles, joint::FullScreenDebugRc_SigmaSmoothTiles);
BenzinDeclareRootResource(Texture2D<float>, g_SigmaPenumbra1, joint::FullScreenDebugRc_SigmaPenumbra1);
BenzinDeclareRootResource(Texture2D<float>, g_SigmaPenumbra2, joint::FullScreenDebugRc_SigmaPenumbra2);
BenzinDeclareRootResource(Texture2D<float>, g_SigmaShadowTemp1, joint::FullScreenDebugRc_SigmaShadowTemp1);
BenzinDeclareRootResource(Texture2D<float>, g_SigmaShadowTemp2, joint::FullScreenDebugRc_SigmaShadowTemp2);
BenzinDeclareRootResource(Texture2D<float>, g_SigmaShadow, joint::FullScreenDebugRc_SigmaShadow);

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
        case joint::DebugOutputType_NoisyPenumbra:
        {
            const float shadowVisibility = noisyPenumbra.SampleLevel(g_PointClampSampler, input.Uv, g_PassConstants.ViewDepthMipIndex);
            return float4(shadowVisibility, 0.0, 0.0, 1.0);
        }
        case joint::DebugOutputType_SigmaTiles:
        {
            const float3 sample = g_SigmaTiles.SampleLevel(g_PointClampSampler, input.Uv, 0.0).xyz;
            return float4(sample, 1.0);
        }
        case joint::DebugOutputType_SigmaSmoothTiles:
        {
            const float2 sample = g_SigmaSmoothTiles.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            return float4(sample, 0.0, 1.0);
        }
        case joint::DebugOutputType_SigmaPenumbra1:
        {
            const float sample = g_SigmaPenumbra1.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            return float4(sample, 0.0, 0.0, 1.0);
        }
        case joint::DebugOutputType_SigmaPenumbra2:
        {
            const float sample = g_SigmaPenumbra2.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            return float4(sample, 0.0, 0.0, 1.0);
        }
        case joint::DebugOutputType_SigmaShadowTemp1:
        {
            const float sample = g_SigmaShadowTemp1.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            return float4(sigma::UnpackShadow(sample), 0.0, 0.0, 1.0);
        }
        case joint::DebugOutputType_SigmaShadowTemp2:
        {
            const float sample = g_SigmaShadowTemp2.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            return float4(sigma::UnpackShadow(sample), 0.0, 0.0, 1.0);
        }
        case joint::DebugOutputType_SigmaShadow:
        {
            const float sample = g_SigmaShadow.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            return float4(sigma::UnpackShadow(sample), 0.0, 0.0, 1.0);
        }
    }

    return float4(1.0, 0.0, 1.0, 1.0);
}
