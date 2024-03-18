#pragma once

struct PackedGBuffer
{
    float4 Color0; // Albedo, Albedo, Albedo, Roughness
    float4 Color1; // Emissive, Emissive, Emissive, Metallic
    float4 Color2; // WorldNormal, WorldNormal, WorldNormal, None
    float4 Color3; // UvMotionVector, UvMotionVector, DepthMotionVector, None
    float4 Color4; // ViewDepth
};

struct UnpackedGBuffer
{
    float3 Albedo;
    float Roughness;
    float3 Emissive;
    float Metallic;
    float3 WorldNormal;
    float2 UvMotionVector;
    float DepthMotionVector;
    float ViewDepth;
};

PackedGBuffer PackGBuffer(UnpackedGBuffer unpacked)
{
    PackedGBuffer packed = (PackedGBuffer)0;
    packed.Color0 = float4(unpacked.Albedo, unpacked.Roughness);
    packed.Color1 = float4(unpacked.Emissive, unpacked.Metallic);
    packed.Color2 = float4(unpacked.WorldNormal, 0.0f);
    packed.Color3 = float4(unpacked.UvMotionVector, unpacked.DepthMotionVector, 0.0f);
    packed.Color4 = float4(unpacked.ViewDepth, 0.0f, 0.0f, 0.0f);

    return packed;
}

UnpackedGBuffer UnpackGBuffer(PackedGBuffer packed)
{
    UnpackedGBuffer unpacked = (UnpackedGBuffer)0;
    unpacked.Albedo = packed.Color0.rgb;
    unpacked.Roughness = packed.Color0.a;
    unpacked.Emissive = packed.Color1.rgb;
    unpacked.Metallic = packed.Color1.a;
    unpacked.WorldNormal = packed.Color2.rgb;
    unpacked.UvMotionVector = packed.Color3.rg;
    unpacked.DepthMotionVector = packed.Color3.b;
    unpacked.ViewDepth = packed.Color4.r;

    return unpacked;
}

float FetchDepth(float2 uv, uint depthTextureIndex)
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[depthTextureIndex];
    return depthTexture.SampleLevel(g_LinearWrapSampler, uv, 0).r;
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

float3 ReconstructWorldPositionFromViewPosition(float3 viewPosition, float4x4 inverseViewMatrix)
{
    const float3 worldPosition = mul(float4(viewPosition, 1.0f), inverseViewMatrix).xyz;
    return worldPosition;
}

