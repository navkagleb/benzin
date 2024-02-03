#pragma once

struct VS_FullScreenTriangleOutput
{
    float4 HomogeneousPosition : SV_Position;
    float4 ClipPosition : ClipPosition;
    float2 UV : UV;
};

float4 GetFullScreenTriangleHomogeneousPosition(uint32_t vertexIndex)
{
    const float x = (float)(vertexIndex >> 1) * -4.0f + 1.0f;
    const float y = (float)(vertexIndex & 1) * -4.0f + 1.0f;

    return float4(x, y, 0.0f, 1.0f);
}

float4 GetFullScreenTriangleHomogeneousPositionDepth1(uint32_t vertexIndex)
{
    float4 homogeneousPosition = GetFullScreenTriangleHomogeneousPosition(vertexIndex);
    homogeneousPosition.z = 1.0f;

    return homogeneousPosition;
}

float2 GetFullScreenTriangleUV(uint32_t vertexID)
{
    const float u = 1.0f - (float)(vertexID >> 1) * 2.0f;
    const float v = (float)(vertexID & 1) * 2.0f;

    return float2(u, v);
}
