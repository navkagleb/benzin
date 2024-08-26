#pragma once

#include "common.hlsli"
#include "sigma_denoiser/sigma_frontend.hlsli"

namespace sigma
{

    // TODO
    // (units) > 0 - use TLAS or tracing range (max value = NRD_FP16_MAX / NRD_FP16_VIEWZ_SCALE - 1 = 524031)
    static const float g_DenoisingRange = 500000.0;

    static const float g_MaxPixelRadius = 16.0; // TODO: at least 32 needed for test 200

    // (normalized %) - represents maximum allowed deviation from local tangent plane
    static const float g_PlaneDistanceSensitivity = 0.005;

    static const float g_PenumbraWeightScale = 10.0;

    // TODO: move to common.hlsli
    uint DivideUp(uint value, uint divisor)
    {
        return (value + divisor - 1) / divisor;
    }

    float3x3 GetOrthonormalBasisFromNormal(float3 normal)
    {
        // Ref: http://marc-b-reynolds.github.io/quaternions/2016/07/06/Orthonormal.html
        // Orthonormal basis from normal via quaternion similarity

        const float sz = sign(normal.z);
        const float a  = 1.0 / (sz + normal.z);
        const float ya = normal.y * a;
        const float b  = normal.x * ya;
        const float c  = normal.x * sz;

        const float3 tangent = float3(c * normal.x * a - 1.0, sz * b, c);
        const float3 bitangent = float3(b, normal.y * ya - sz, normal.y);

        // Note: due to the quaternion formulation, the generated frame is rotated by 180 degrees,
        // s.t. if N = (0, 0, 1), then T = (-1, 0, 0) and B = (0, -1, 0).
        return float3x3(tangent, bitangent, normal);
    }

    struct LdsDistributor
    {
        uint2 ThreadPos;
        uint2 PixelPos;
        uint FlatThreadIndex;

        uint BorderSize;

        uint2 GroupSize;
        uint2 BufferSize;

        uint2 Dimension;

        template <typename T>
        void Preload(T preloadType)
        {
            const int2 groupBasePos = PixelPos - ThreadPos - BorderSize; // Is this equal to SV_GroupID?
            const uint stageCount = DivideUp(BufferSize.x * BufferSize.y, GroupSize.x * GroupSize.y);
    
            [unroll]
            for (uint stageIndex = 0; stageIndex < stageCount; ++stageIndex)
            {
                const uint flatTileIndex = FlatThreadIndex + stageIndex * GroupSize.x * GroupSize.y;
                const uint2 localTilePos = uint2(flatTileIndex % BufferSize.x, flatTileIndex / BufferSize.y);

                if (stageIndex == 0 || flatTileIndex < BufferSize.x * BufferSize.y)
                {
                    const uint2 globalTilePos = clamp(groupBasePos + localTilePos, 0, Dimension - 1);
                    preloadType.Preload(localTilePos, globalTilePos);
                }
            }
        }
    };

    bool IsLit(float penumbra)
    {
        return penumbra >= sigma::g_Fp16Max;
    }

    float PixelRadiusToWorldAtDepth(float pixelRadius, float viewDepth)
    {
        return pixelRadius * g_FrameConstants.PixelToWorldScale * viewDepth;
    }

    float GetFrustumSizeAtDepth(float pixelToWorldScale, float minRenderSize, float viewDepth)
    {
        const float minViewportSideSize = min(g_FrameConstants.RenderResolution.x, g_FrameConstants.RenderResolution.y);

        return minRenderSize * pixelToWorldScale * viewDepth;
    }

    float GetKernelRadiusInPixels(float hitDistance, float unprojectDepth, float scale = 1.0)
    {
        const float unclampedRadius = hitDistance / unprojectDepth;
        const float minRadius = min(unclampedRadius, SIGMA_BORDER);

        return clamp(unclampedRadius * scale, minRadius, g_MaxPixelRadius);
    }

    float2 GetGeometryWeightParams(float planeDistanceSensitivity, float frustumSize, float3 viewPos, float3 viewNormal, float nonLinearAccumSpeed)
    {
        const float relaxation = lerp(1.0, 0.25, nonLinearAccumSpeed); // => 0.25
        const float a = relaxation / (planeDistanceSensitivity * frustumSize);
        const float b = -dot(viewNormal, viewPos) * a;

        return float2(a, b);
    }

    float ComputeWeight(float x, float px, float py)
    {
        // A good choice for non noisy data
        // IMPORTANT: cutoffs are needed to minimize floating point precision drifting
        return smoothstep(0.999, 0.001, abs((x) * px + py));
    }

    float GetGaussianWeight(float r)
    {
        // It's Gaussian blur weight or simple Gaussian weight ???
        return exp(-0.66 * r * r); // assuming r is normalized to 1
    }

    float2 FilterBicubic(float2 size, float2 uv, out float4 outUv_10_00, out float4 outUv_11_01)
    {
        const float4 c1 = float4(3.0, 0.0, 1.0, 4.0);
        const float4 c2 = float4(-1.0, 3.0, -3.0, 1.0);
        const float4 c3 = float4(3.0, -6.0, -3.0, 0.0);
        const float k = 1.0 / 6.0;

        const float4 dxdy = -c1.zyyz / size.xyxy;

        const float2 f = frac(uv.xy * size - 0.5);
        const float2 f2 = f * f;
        const float2 f3 = f2 * f;

        float3 xw;
        float3 yw;
        float4 phi;

        phi = k * (c2.xyzw * f3.xxxx + c3.xyxw * f2.xxxx + c3.zwxw * f.xxxx + c1.zwzy);
        xw.xy = c2.ww + c2.wx * f.xx + c2.xw * phi.yw / ( phi.xz + phi.yw );
        xw.z = phi.x + phi.y;

        phi = k * (c2.xyzw * f3.yyyy + c3.xyxw * f2.yyyy + c3.zwxw * f.yyyy + c1.zwzy);
        yw.xy = c2.ww + c2.wx * f.yy + c2.xw * phi.yw / (phi.xz + phi.yw);
        yw.z = phi.x + phi.y;

        outUv_10_00 = uv.xyxy + c2.wwxx * xw.xxyy * dxdy.xyxy;
        outUv_11_01 = outUv_10_00 + yw.xxxx * dxdy.zwzw;

        outUv_10_00 -= yw.yyyy * dxdy.zwzw;

        return float2(yw.z, xw.z);
    }

    float TextureCubic(Texture2D<float2> tex, float2 uv)
    {
        uint w, h;
        tex.GetDimensions(w, h);
        const float2 size = float2(w, h);

        float4 uv_10_00, uv_11_01;
        const float2 t = FilterBicubic(size, uv.xy, uv_10_00, uv_11_01);

        float c00 = tex.SampleLevel(g_LinearClampSampler, uv_10_00.zw, 0).x;
        float c10 = tex.SampleLevel(g_LinearClampSampler, uv_10_00.xy, 0).x;
        float c01 = tex.SampleLevel(g_LinearClampSampler, uv_11_01.zw, 0).x;
        float c11 = tex.SampleLevel(g_LinearClampSampler, uv_11_01.xy, 0).x;

        const float horizontalLerp0 = lerp(c00, c01, t.x);
        const float horizontalLerp1 = lerp(c10, c11, t.x);

        return lerp(horizontalLerp0, horizontalLerp1, t.y);
    }

}
