// Ref: https://wojtsterna.blogspot.com/2018/02/directx-11-hlsl-gatherred.html

struct BilinearFilter
{
    float2 TexelSize;
    float2 TopLeftTexelPosition;
    float2 Weights;
};

BilinearFilter CreateBilinearFilter(float2 uv, float2 textureSize)
{
    const float2 textureOffsetEpsilon = textureSize * 0.000001;
    const float2 texelPosition = (uv * textureSize) - 0.5 + textureOffsetEpsilon; // Force jump to the correct texel

    BilinearFilter filter;
    filter.TexelSize = 1.0 / textureSize;
    filter.TopLeftTexelPosition = floor(texelPosition);
    filter.Weights = frac(texelPosition);

    return filter;
}

float4 GatherRedManually(Texture2D<float> texture, BilinearFilter filter, uint mipIndex = 0)
{
    // w z
    // x y
    // uv - points to 'w' texel
    // For gathering uv should point to 'y' texel

    const float2 uv = filter.TopLeftTexelPosition * filter.TexelSize;
    
    float4 samples = 0.0;
    samples.x = texture.SampleLevel(g_PointWithTransparentBlackBorderSampler, uv, mipIndex);
    samples.y = texture.SampleLevel(g_PointWithTransparentBlackBorderSampler, uv + float2(filter.TexelSize.x, 0.0), mipIndex);
    samples.z = texture.SampleLevel(g_PointWithTransparentBlackBorderSampler, uv + float2(0.0, filter.TexelSize.y), mipIndex);
    samples.w = texture.SampleLevel(g_PointWithTransparentBlackBorderSampler, uv + filter.TexelSize, mipIndex);

    return samples;
}

float4 ExpandBilinearWeights(BilinearFilter filter)
{
    const float2 weights = filter.Weights;

    // Expand 3 lerps into separate weights for each sample of the 2x2 footprint
    // lerp(
    //     lerp(w, z, weights.x),
    //     lerp(x, y, weights.x),
    //     weights.y
    // )

    float4 expandedWeights;
    expandedWeights.x = (1.0 - weights.x) * (1.0 - weights.y);
    expandedWeights.y = weights.x * (1.0 - weights.y);
    expandedWeights.z = (1.0 - weights.x) * weights.y;
    expandedWeights.w = weights.x * weights.y;

    return expandedWeights;
}

float ApplyBilinearCustomWeights(BilinearFilter filter, float4 gatheredValues, float4 customWeights)
{
    const float4 weights = ExpandBilinearWeights(filter) * customWeights;
    const float weightSum = dot(weights, 1.0);

    if (abs(weightSum) < 1.0e-5)
    {
        return 0.0;
    }

    const float gatheredSum = dot(gatheredValues * weights, 1.0);
    return gatheredSum * rcp(weightSum); // Normalization
}
