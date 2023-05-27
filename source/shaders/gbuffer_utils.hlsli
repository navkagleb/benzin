#pragma once

namespace gbuffer
{

    struct Packed
    {
        float4 Color0; // Albedo, Albedo, Albedo, Albedo
        float4 Color1; // WorldNormal, WorldNormal, WorldNormal, None
        float4 Color2; // Emissive, Emissive, Emissive, None
        float4 Color3; // Roughness, Metalness, None, None
    };

    struct Unpacked
    {
        float4 Albedo;
        float3 WorldNormal;
        float Roughness;
        float3 Emissive;
        float Metalness;
    };

    Packed Pack(Unpacked unpacked)
    {
        Packed packed = (Packed)0;
        packed.Color0 = float4(unpacked.Albedo);
        packed.Color1 = float4(unpacked.WorldNormal, 0.0f);
        packed.Color2 = float4(unpacked.Emissive, 0.0f);
        packed.Color3 = float4(unpacked.Roughness, unpacked.Metalness, 0.0f, 0.0f);

        return packed;
    }

    Unpacked Unpack(Packed packed)
    {
        Unpacked unpacked = (Unpacked)0;
        unpacked.Albedo = packed.Color0;
        unpacked.WorldNormal = packed.Color1.xyz;
        unpacked.Emissive = packed.Color2.rgb;
        unpacked.Roughness = packed.Color3.r;
        unpacked.Metalness = packed.Color3.g;

        return unpacked;
    }

    float3 ReconstructViewPositionFromDepth(float2 uv, float depth, float4x4 inverseProjectionMatrix)
    {
        // Get x/w and y/w from the viewport position
        const float x = uv.x * 2.0f - 1.0f;
        const float y = (1.0f - uv.y) * 2.0f - 1.0f;
        const float z = depth;

        const float4 ndcPosition = float4(x, y, z, 1.0f);
        const float4 viewPosition = mul(ndcPosition, inverseProjectionMatrix);

        return viewPosition.xyz / viewPosition.w;
    }

    float3 ReconstructWorldPositionFromDepth(float2 uv, float depth, float4x4 inverseProjectionMatrix, float4x4 inverseViewMatrix)
    {
        const float3 viewPosition = ReconstructViewPositionFromDepth(uv, depth, inverseProjectionMatrix);
        const float3 worldPosition = mul(float4(viewPosition, 1.0f), inverseViewMatrix).xyz;

        return worldPosition;
    }

} // namespace gbuffer