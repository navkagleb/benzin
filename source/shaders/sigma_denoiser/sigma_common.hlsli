#include "common.hlsli"
#include "sigma_denoiser/sigma_constants.hlsli"
#include "sigma_denoiser/sigma_public.hlsli"
#include "space_convertions.hlsli"

namespace sigma
{

    float PixelsToWorldSize(uint pixelCount, float pixelToWorldScale, float viewDepth)
    {
        // 'pixelToWorldScale' is used to account for render viewport resolution
        // 'viewDepth' is used to account for perspective projection
        return (float)pixelCount * pixelToWorldScale * viewDepth;
    }

    float GetWorldPixelSize(float pixelToWorldScale, float viewDepth)
    {
        return PixelsToWorldSize(1, pixelToWorldScale, viewDepth);
    }

    float GetKernelPixelRadius(float penumbra, float worldPixelSize, float pixelScale = 1.0)
    {
        float pixelRadius = penumbra / worldPixelSize; // Larger 'penumbra' or smaller 'worldPixelSize' increases the radius.
        pixelRadius *= pixelScale;

#if defined(SIGMA_USE_BORDER_2)
        const float minRadius = min(pixelRadius, 2.0);
#else
        const float minRadius = min(pixelRadius, 1.0);
#endif

        return clamp(pixelRadius, minRadius, SIGMA_MAX_BLUR_KERNEL_PIXEL_RADIUS);
    }

    bool IsBothLitOrUmbra(float penumbra1, float penumbra2)
    {
        // Check the tile classification (penumbra value meaning)
        const bool isLitOrUmbra1 = penumbra1 == 0.0;
        const bool isLitOrUmbra2 = penumbra2 == 0.0;

        return isLitOrUmbra1 == isLitOrUmbra2;
    }

    float2 RotateVectorByRotator(float2 vector2, float4 rotator)
    {
        // Rotator - rotation matrix 2x2
        return vector2.x * rotator.xz + vector2.y * rotator.yw;
    }

    bool IsUvIn01Range(float2 uv)
    {
        return float(all(uv >= 0.0) && all(uv < 1.0));
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

    float2 GetGeometryWeightParams(float planeDistanceSensitivity, float3 viewPos, float3 viewNormal, float worldFrustumSize)
    {
        const float surfaceViewAlignment = dot(viewNormal, viewPos);

        const float scale = 1.0 / (planeDistanceSensitivity * worldFrustumSize);
        const float bias = surfaceViewAlignment * scale;

        return float2(scale, -bias);
    }

    float CalcGeometryWeight(float3 viewNormal, float3 viewPos, float2 scaleAndBias)
    {
        // A good choice for non noisy data
        // IMPORTANT: cutoffs are needed to minimize floating point precision drifting

        const float surfaceViewAlignment = dot(viewNormal, viewPos);

        return smoothstep(1.0, 0.0, abs(surfaceViewAlignment * scaleAndBias.x + scaleAndBias.y));
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

    float TextureCubicX(Texture2D<float2> tex, float2 uv)
    {
        uint width;
        uint height;
        tex.GetDimensions(width, height);

        float4 uv_10_00, uv_11_01;
        const float2 t = FilterBicubic(float2(width, height), uv.xy, uv_10_00, uv_11_01);

        float c00 = tex.SampleLevel(g_LinearClampSampler, uv_10_00.zw, 0).x;
        float c10 = tex.SampleLevel(g_LinearClampSampler, uv_10_00.xy, 0).x;
        float c01 = tex.SampleLevel(g_LinearClampSampler, uv_11_01.zw, 0).x;
        float c11 = tex.SampleLevel(g_LinearClampSampler, uv_11_01.xy, 0).x;

        const float horizontalLerp0 = lerp(c00, c01, t.x);
        const float horizontalLerp1 = lerp(c10, c11, t.x);

        return lerp(horizontalLerp0, horizontalLerp1, t.y);
    }

}
