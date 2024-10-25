#pragma once

static const float g_CatRomSharpness = 0.5; // [ 0; 1 ], 0.5 matches Catmull-Rom

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

#define _BicubicFilterNoCornersWithFallbackToBilinearFilterWithCustomWeights_Color( color, tex ) \
    /* Sampling */ \
    color = tex.SampleLevel( gLinearClamp, uv01.xy, 0 ) * w.x; \
    color += tex.SampleLevel( gLinearClamp, uv01.zw, 0 ) * w.y; \
    color += tex.SampleLevel( gLinearClamp, uv23.xy, 0 ) * w.z; \
    color += tex.SampleLevel( gLinearClamp, uv23.zw, 0 ) * w.w; \
    color += tex.SampleLevel( gLinearClamp, uv4, 0 ) * w4; \
    /* Normalize similarly to "Filtering::ApplyBilinearCustomWeights()" */ \
    color = sum < 0.0001 ? 0 : color / sum;

float4 BicubicFilterNoCorners(Texture2D<float4> bicubicTexture, float2 texelPosition, float2 invTextureSize)
{
    // Catmul-Rom with 12 taps ( excluding corners )

    const float2 centerPosition = floor(texelPosition - 0.5) + 0.5;
    const float2 f = saturate(texelPosition - centerPosition);

    const float2 weight0 = f * (f * (-g_CatRomSharpness * f + 2.0 * g_CatRomSharpness) - g_CatRomSharpness);
    const float2 weight1 = f * (f * ((2.0 - g_CatRomSharpness) * f - (3.0 - g_CatRomSharpness))) + 1.0;
    const float2 weight2 = f * (f * (-(2.0 - g_CatRomSharpness) * f + (3.0 - 2.0 * g_CatRomSharpness)) + g_CatRomSharpness);
    const float2 weight3 = f * (f * (g_CatRomSharpness * f - g_CatRomSharpness));

    const float2 weight12 = weight1 + weight2;
    const float2 tc = weight2 / weight12;

    float4 customWeights = 0.0;
    customWeights.x = weight12.x * weight0.y;
    customWeights.y = weight0.x * weight12.y;
    customWeights.z = weight12.x * weight12.y;
    customWeights.w = weight3.x * weight12.y;

    const float weight4 = weight12.x * weight3.y;

    // Fallback to custom bilinear
    const float weightSum = dot(customWeights, 1.0) + weight4;

    // Texture coordinates
    float4 uv01 = centerPosition.xyxy + float4(tc.x, -1.0, -1.0, tc.y);
    float4 uv23 = centerPosition.xyxy + float4(tc.x, tc.y, 2.0, tc.y);
    float2 uv4 = centerPosition + float2( tc.x, 2.0 );

    uv01 *= invTextureSize.xyxy;
    uv23 *= invTextureSize.xyxy;
    uv4 *= invTextureSize;

    // Sampling
    float4 color = 0.0;
    color += bicubicTexture.SampleLevel(g_LinearClampSampler, uv01.xy, 0.0) * customWeights.x;
    color += bicubicTexture.SampleLevel(g_LinearClampSampler, uv01.zw, 0.0) * customWeights.y;
    color += bicubicTexture.SampleLevel(g_LinearClampSampler, uv23.xy, 0.0) * customWeights.z;
    color += bicubicTexture.SampleLevel(g_LinearClampSampler, uv23.zw, 0.0) * customWeights.w;
    color += bicubicTexture.SampleLevel(g_LinearClampSampler, uv4, 0.0) * weight4;
        
    // Normalize similarly to "Filtering::ApplyBilinearCustomWeights()"
    color = weightSum < 0.0001 ? 0.0 : color / weightSum;

    return color;
}
