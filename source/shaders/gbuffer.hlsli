#pragma once

#include "space_convertions.hlsli"
#include "unified_root_parameters.hlsli"

struct PackedGBuffer
{
    float4 m_Color0 : SV_Target0; // Albedo, Albedo, Albedo, Roughness
    float4 m_Color1 : SV_Target1; // Emissive, Emissive, Emissive, Metallic
    float4 m_Color2 : SV_Target2; // WorldNormal, WorldNormal, WorldNormal, None
    float4 m_Color3 : SV_Target3; // UvMv, UvMv, ViewDepthMv, None
    float4 m_Color4 : SV_Target4; // ViewDepth
};

struct GBuffer
{
    float3 m_Albedo;
    float m_Roughness;
    float3 m_Emissive;
    float m_Metallic;
    float3 m_WorldNormal;
    float m_ViewDepth;

    float2 m_UvMv;
    float m_ViewDepthMv;
};

PackedGBuffer PackGBuffer(GBuffer unpacked)
{
    PackedGBuffer packed = (PackedGBuffer)0;
    packed.m_Color0 = float4(unpacked.m_Albedo, unpacked.m_Roughness);
    packed.m_Color1 = float4(unpacked.m_Emissive, unpacked.m_Metallic);
    packed.m_Color2 = float4(unpacked.m_WorldNormal, 0.0f);
    packed.m_Color3 = float4(unpacked.m_UvMv, unpacked.m_ViewDepthMv, 0.0f);
    packed.m_Color4 = float4(unpacked.m_ViewDepth, 0.0f, 0.0f, 0.0f);

    return packed;
}

GBuffer UnpackGBuffer(PackedGBuffer packed)
{
    GBuffer unpacked = (GBuffer)0;
    unpacked.m_Albedo = packed.m_Color0.rgb;
    unpacked.m_Roughness = packed.m_Color0.a;
    unpacked.m_Emissive = packed.m_Color1.rgb;
    unpacked.m_Metallic = packed.m_Color1.a;
    unpacked.m_WorldNormal = packed.m_Color2.rgb;
    unpacked.m_ViewDepth = packed.m_Color4.r;
    unpacked.m_UvMv = packed.m_Color3.rg;
    unpacked.m_ViewDepthMv = packed.m_Color3.b;

    return unpacked;
}

void CalcGBufferMv(float2 pixelPos, float viewDepth, float3 prevViewPos, out GBuffer outGBuffer)
{
    const float4 prevClipPos = mul(float4(prevViewPos, 1.0), GetPrevCameraConsts().ViewToClip);

    const float2 uv = pixelPos * g_FrameConsts.InvRenderResolution;
    const float2 prevUv = ClipToUv(prevClipPos);

    outGBuffer.m_UvMv = (uv - prevUv) * g_FrameConsts.RenderResolution; // TODO: Pack/Unpack Mv
    outGBuffer.m_ViewDepthMv = viewDepth - prevViewPos.z;
}
