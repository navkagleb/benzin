#pragma once

#include "space_convertions.hlsli"
#include "unified_root_parameters.hlsli"

struct PackedGBuffer
{
    float4 Color0 : SV_Target0; // Albedo, Albedo, Albedo, Roughness
    float4 Color1 : SV_Target1; // Emissive, Emissive, Emissive, Metallic
    float4 Color2 : SV_Target2; // WorldNormal, WorldNormal, WorldNormal, None
    float4 Color3 : SV_Target3; // UvMv, UvMv, ViewDepthMv, None
    float4 Color4 : SV_Target4; // ViewDepth
};

struct GBuffer
{
    float3 Albedo;
    float Roughness;
    float3 Emissive;
    float Metallic;
    float3 WorldNormal;
    float ViewDepth;

    float2 UvMv;
    float ViewDepthMv;
};

PackedGBuffer PackGBuffer(GBuffer unpacked)
{
    PackedGBuffer packed = (PackedGBuffer)0;
    packed.Color0 = float4(unpacked.Albedo, unpacked.Roughness);
    packed.Color1 = float4(unpacked.Emissive, unpacked.Metallic);
    packed.Color2 = float4(unpacked.WorldNormal, 0.0f);
    packed.Color3 = float4(unpacked.UvMv, unpacked.ViewDepthMv, 0.0f);
    packed.Color4 = float4(unpacked.ViewDepth, 0.0f, 0.0f, 0.0f);

    return packed;
}

GBuffer UnpackGBuffer(PackedGBuffer packed)
{
    GBuffer unpacked = (GBuffer)0;
    unpacked.Albedo = packed.Color0.rgb;
    unpacked.Roughness = packed.Color0.a;
    unpacked.Emissive = packed.Color1.rgb;
    unpacked.Metallic = packed.Color1.a;
    unpacked.WorldNormal = packed.Color2.rgb;
    unpacked.ViewDepth = packed.Color4.r;
    unpacked.UvMv = packed.Color3.rg;
    unpacked.ViewDepthMv = packed.Color3.b;

    return unpacked;
}

void CalcGBufferMv(float2 pixelPos, float viewDepth, float3 prevViewPos, out GBuffer outGBuffer)
{
    const float4 prevClipPos = mul(float4(prevViewPos, 1.0), GetPrevCameraConsts().ViewToClip);

    const float2 uv = pixelPos * g_FrameConsts.InvRenderResolution;
    const float2 prevUv = ClipToUv(prevClipPos);

    outGBuffer.UvMv = (uv - prevUv) * g_FrameConsts.RenderResolution; // TODO: Pack/Unpack Mv
    outGBuffer.ViewDepthMv = viewDepth - prevViewPos.z;
}
