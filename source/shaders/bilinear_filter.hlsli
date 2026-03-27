#pragma once

#include "unified_root_parameters.hlsli"

// Ref: https://wojtsterna.blogspot.com/2018/02/directx-11-hlsl-gatherred.html

struct BilinearFilter
{
    float2 TopLeftTexelPos;
    float2 LerpWeights;
};

BilinearFilter CreateBilinearFilter(float2 uv, float2 textureSize)
{
    const float2 texelPos = (uv * textureSize) - 0.5;

    BilinearFilter filter;
    filter.TopLeftTexelPos = floor(texelPos);
    filter.LerpWeights = frac(texelPos);

    return filter;
}

float4 ExpandBilinearWeights(float2 weights)
{
    // Expand 3 lerps into separate weights for each sample of the 2x2 footprint
    // lerp(
    //     lerp(w, z, weights.x),
    //     lerp(x, y, weights.x),
    //     weights.y
    // )

    const float2 oneMinusWeights = saturate(1.0 - weights);

    float4 expandedWeights;
    expandedWeights.x = oneMinusWeights.x * oneMinusWeights.y;
    expandedWeights.y = weights.x * oneMinusWeights.y;
    expandedWeights.z = oneMinusWeights.x * weights.y;
    expandedWeights.w = weights.x * weights.y;

    return expandedWeights;
}

float ApplyBilinearCustomWeights(float4 gatheredValues, float4 weights)
{
    const float weightSum = dot(weights, 1.0);

    if (abs(weightSum) < 0.0001)
    {
        return 0.0;
    }

    const float gatheredSum = dot(gatheredValues * weights, 1.0);
    return gatheredSum * rcp(weightSum);
}

#define NRD_CATROM_SHARPNESS 0.5

#define _BicubicFilterNoCornersWithFallbackToBilinearFilterWithCustomWeights_Init \
    /* Catmul-Rom with 12 taps ( excluding corners ) */ \
    float2 centerPos = floor( samplePos - 0.5 ) + 0.5; \
    float2 f = saturate( samplePos - centerPos ); \
    float2 w0 = f * ( f * ( -NRD_CATROM_SHARPNESS * f + 2.0 * NRD_CATROM_SHARPNESS ) - NRD_CATROM_SHARPNESS ); \
    float2 w1 = f * ( f * ( ( 2.0 - NRD_CATROM_SHARPNESS ) * f - ( 3.0 - NRD_CATROM_SHARPNESS ) ) ) + 1.0; \
    float2 w2 = f * ( f * ( -( 2.0 - NRD_CATROM_SHARPNESS ) * f + ( 3.0 - 2.0 * NRD_CATROM_SHARPNESS ) ) + NRD_CATROM_SHARPNESS ); \
    float2 w3 = f * ( f * ( NRD_CATROM_SHARPNESS * f - NRD_CATROM_SHARPNESS ) ); \
    float2 w12 = w1 + w2; \
    float2 tc = w2 / w12; \
    float4 w; \
    w.x = w12.x * w0.y; \
    w.y = w0.x * w12.y; \
    w.z = w12.x * w12.y; \
    w.w = w3.x * w12.y; \
    float w4 = w12.x * w3.y; \
    /* Fallback to custom bilinear */ \
    w = useBicubic ? w : bilinearCustomWeights; \
    w4 = useBicubic ? w4 : 0.0; \
    float sum = dot( w, 1.0 ) + w4; \
    /* Texture coordinates */ \
    float4 uv01 = centerPos.xyxy + ( useBicubic ? float4( tc.x, -1.0, -1.0, tc.y ) : float4( 0, 0, 1, 0 ) ); \
    float4 uv23 = centerPos.xyxy + ( useBicubic ? float4( tc.x, tc.y, 2.0, tc.y ) : float4( 0, 1, 1, 1 ) ); \
    float2 uv4 = centerPos + ( useBicubic ? float2( tc.x, 2.0 ) : f ); \
    uv01 *= invResourceSize.xyxy; \
    uv23 *= invResourceSize.xyxy; \
    uv4 *= invResourceSize; \
    int3 bilinearOrigin = int3( centerPos, 0 );

/*
IMPORTANT:
- 0 can be returned if only a single tap is valid from the 2x2 footprint and pure bilinear weights
  are close to 0 near this tap. The caller must handle this case manually. "Footprint quality"
  can be used to accelerate accumulation and avoid the problem.
- can return negative values
*/
#define _BicubicFilterNoCornersWithFallbackToBilinearFilterWithCustomWeights_Color( color, tex ) \
    /* Sampling */ \
    color = tex.SampleLevel( g_LinearClampSampler, uv01.xy, 0 ) * w.x; \
    color += tex.SampleLevel( g_LinearClampSampler, uv01.zw, 0 ) * w.y; \
    color += tex.SampleLevel( g_LinearClampSampler, uv23.xy, 0 ) * w.z; \
    color += tex.SampleLevel( g_LinearClampSampler, uv23.zw, 0 ) * w.w; \
    color += tex.SampleLevel( g_LinearClampSampler, uv4, 0 ) * w4; \
    /* Normalize similarly to "Filtering::ApplyBilinearCustomWeights()" */ \
    color = sum < 0.0001 ? 0 : color / sum;

void BicubicFilterNoCornersWithFallbackToBilinearFilterWithCustomWeights(
    float2 samplePos,
    float2 invResourceSize,
    float4 bilinearCustomWeights,
    bool useBicubic,
    Texture2D<float> tex0,
    out float c0
)
{
    _BicubicFilterNoCornersWithFallbackToBilinearFilterWithCustomWeights_Init;
    _BicubicFilterNoCornersWithFallbackToBilinearFilterWithCustomWeights_Color(c0, tex0);
}
