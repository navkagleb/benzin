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
