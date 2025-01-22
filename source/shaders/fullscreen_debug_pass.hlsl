#include "joint/full_screen_debug_resources.hpp"
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

BenzinDeclareRootResource(Texture2D<float4>, g_AlbedoAndRoughness, joint::Rc_FullScreenDebug::AlbedoAndRoughness);
BenzinDeclareRootResource(Texture2D<float4>, g_EmissiveAndMetallic, joint::Rc_FullScreenDebug::EmissiveAndMetallic);
BenzinDeclareRootResource(Texture2D<float4>, g_WorldNormal, joint::Rc_FullScreenDebug::WorldNormal);
BenzinDeclareRootResource(Texture2D<float2>, g_Mv, joint::Rc_FullScreenDebug::Mv);
BenzinDeclareRootResource(Texture2D<float>, g_ViewDepth, joint::Rc_FullScreenDebug::ViewDepth);
BenzinDeclareRootResource(Texture2D<float>, g_Depth, joint::Rc_FullScreenDebug::Depth);
BenzinDeclareRootResource(Texture2D<float>, g_NoisyPenumbra, joint::Rc_FullScreenDebug::NoisyPenumbra);

float4 PsMain(VsFullScreenTriangleOutput input) : SV_Target
{
    PackedGBuffer packedGBuffer;
    packedGBuffer.Color0 = g_AlbedoAndRoughness.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    packedGBuffer.Color1 = g_EmissiveAndMetallic.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    packedGBuffer.Color2 = g_WorldNormal.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    packedGBuffer.Color3 = float4(g_Mv.SampleLevel(g_PointClampSampler, input.Uv, 0.0), 0.0, 0.0);

    UnpackedGBuffer gbuffer = UnpackGBuffer(packedGBuffer);

    const float depth = g_Depth.SampleLevel(g_PointClampSampler, input.Uv, 0);

    // if (depth == 1.0f)
    // {
    //     discard;
    // }

    switch (g_PassConsts0.OutputType)
    {
        case joint::DebugOutputType::GBuffer_Albedo: return float4(gbuffer.Albedo, 1.0);
        case joint::DebugOutputType::GBuffer_Roughness: return float4(gbuffer.Roughness.xxx, 1.0);
        case joint::DebugOutputType::GBuffer_Emissive: return float4(gbuffer.Emissive, 1.0);
        case joint::DebugOutputType::GBuffer_Metallic: return float4(gbuffer.Metallic.xxx, 1.0);
        case joint::DebugOutputType::GBuffer_WorldNormal: return float4(gbuffer.WorldNormal, 1.0);
        case joint::DebugOutputType::GBuffer_ViewDepth:
        {
            const float viewDepth = g_ViewDepth.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            const float3 viewPos = ReconstructViewPosition(input.Uv, viewDepth, g_FrameConstants.Camera.UvToViewScale, g_FrameConstants.Camera.UvToViewBias);
            return float4(viewPos, 1.0);

            // const float viewDepth = viewDepthBuffer.SampleLevel(g_PointClampSampler, input.Uv, g_PassConsts0.ViewDepthMipIndex);
            // return float4((viewDepth + g_PassConsts0.MinViewDepth) / g_PassConsts0.MaxViewDepth, 0.0, 0.0, 1.0);
        }
        default: break;
    }

    return float4(1.0, 0.0, 1.0, 1.0);
}
