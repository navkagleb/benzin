#pragma once

// NOTE: include this file after "unified_root_parameters.hlsli" include file !!!

#include "space_convertions.hlsli"

float ExtractScale(float4x4 transform)
{
    const float scaleX = length(transform[0].xyz);
    const float scaleY = length(transform[1].xyz);
    const float scaleZ = length(transform[2].xyz);

    return max(max(scaleX, scaleY), scaleZ);
}

// Sources:
// - Two-Pass Hierarchical Z-Buffer Occlusion Culling: https://medium.com/@Lucmomber/two-pass-hierarchical-z-buffer-occlusion-culling-93171c5a9808
// - Depth Precision Visualized (Reversed-Z): https://developer.nvidia.com/content/depth-precision-visualized
// - GDC 2024 - Mesh Shaders in AMD RDNA™ 3 Architecture: https://www.youtube.com/watch?v=MQv76-q2cm8
// - milkru/vulkanizer: https://github.com/milkru/vulkanizer/blob/main/src/shaders/generate_draws.comp
// - TODO - Using Mesh Shaders for Professional Graphics: https://developer.nvidia.com/blog/using-mesh-shaders-for-professional-graphics/
// - TODO - NVIDIA Sharing New Details about Mesh Shading at SIGGRAPH 2019: https://developer.nvidia.com/blog/siggraph-2019-mesh-shading-talk/
// - TODO - Direct3D 12: Long Way to Access Data: https://asawicki.info/news_1754_direct3d_12_long_way_to_access_data
// - TODO - Efficient Use of GPU Memory in Modern Games - Digital Dragons 2021: https://gpuopen.com/videos/efficient-use-of-gpu-memory-digital-dragons/
// - TODO - D3D12 Memory Allocator: https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator

// Reserved depth buffer
// - 1 Near
// - 0 Far

struct ScreenBounds
{
    float2 UvMin;
    float2 UvMax;
    float NearestDepth;
};

ScreenBounds CalcScreenBounds(float3 worldCenter, float worldRadius)
{
    ScreenBounds bounds;
    bounds.UvMin = 1.0;
    bounds.UvMax = 0.0;
    bounds.NearestDepth = 0.0;

    const float3 worldOffsets[8] =
    {
        float3(-worldRadius, -worldRadius, -worldRadius),
        float3(-worldRadius, -worldRadius, +worldRadius),
        float3(-worldRadius, +worldRadius, -worldRadius),
        float3(-worldRadius, +worldRadius, +worldRadius),
        float3(+worldRadius, -worldRadius, -worldRadius),
        float3(+worldRadius, -worldRadius, +worldRadius),
        float3(+worldRadius, +worldRadius, -worldRadius),
        float3(+worldRadius, +worldRadius, +worldRadius),
    };

    [unroll]
    for (uint i = 0; i < 8; ++i)
    {
        const float3 worldCorner = worldCenter + worldOffsets[i];
        const float4 clipCorner = mul(float4(worldCorner, 1.0), GetCameraConsts().WorldToClip);

        const float3 ndcCorner = clipCorner.xyz / clipCorner.w;
        const float2 uvCorner = ndcCorner.xy * 0.5 + 0.5;

        bounds.UvMin = min(bounds.UvMin, uvCorner);
        bounds.UvMax = max(bounds.UvMax, uvCorner);
        bounds.NearestDepth = max(bounds.NearestDepth, ndcCorner.z); // Reversed depth buffer -> max
    }

    return bounds;
}

uint CalcHzbMipLevel(ScreenBounds bounds)
{
    const float2 uvSize = bounds.UvMax - bounds.UvMin;
    const float2 pixelSize = g_FrameConsts.RenderResolution * uvSize;

    const uint mipLevel = (uint)ceil(log2(max(pixelSize.x, pixelSize.y)));
    const uint maxMipLevel = (uint)log2(max(g_FrameConsts.RenderResolution.x, g_FrameConsts.RenderResolution.y));
    return clamp(mipLevel, 0, maxMipLevel);
}

float SampleHzb(Texture2D<float> hzb, ScreenBounds bounds, uint mipLevel)
{
    const float4 uvBox = float4(bounds.UvMin, bounds.UvMax);

#if 1 // Not the solution
    uint2 mipSize;
    uint mipCount;
    hzb.GetDimensions(mipLevel, mipSize.x, mipSize.y, mipCount);

    uint2 texel0 = uint2(uvBox.xy * mipSize);
    uint2 texel1 = uint2(uvBox.zy * mipSize);
    uint2 texel2 = uint2(uvBox.xw * mipSize);
    uint2 texel3 = uint2(uvBox.zw * mipSize);

    const float depth0 = hzb.Load(int3(texel0, mipLevel));
    const float depth1 = hzb.Load(int3(texel1, mipLevel));
    const float depth2 = hzb.Load(int3(texel2, mipLevel));
    const float depth3 = hzb.Load(int3(texel3, mipLevel));
#else
    const float depth0 = hzb.SampleLevel(g_PointClampSampler, uvBox.xy, mipLevel);
    const float depth1 = hzb.SampleLevel(g_PointClampSampler, uvBox.zy, mipLevel);
    const float depth2 = hzb.SampleLevel(g_PointClampSampler, uvBox.xw, mipLevel);
    const float depth3 = hzb.SampleLevel(g_PointClampSampler, uvBox.zw, mipLevel);
#endif

    const float farthestDepth = min(min(depth0, depth1), min(depth2, depth3)); // Reversed depth buffer -> min
    return farthestDepth;
}

bool IsOcclusionCulled(Texture2D<float> hzb, ScreenBounds bounds)
{
    const uint mipLevel = CalcHzbMipLevel(bounds);
    const float farthestDepth = SampleHzb(hzb, bounds, mipLevel);

    const float depthEps = 1e-4;
    return bounds.NearestDepth < farthestDepth - depthEps;
}
