#pragma once

struct VsFullScreenTriangleOutput
{
    float4 SvPosition : SV_Position;
    float4 ClipPosition : ClipPosition;
    float2 Uv : Uv;
};

float4 GetFullScreenTriangleClipPosition(uint32_t vertexIndex)
{
    const float x = (float)(vertexIndex >> 1) * -4.0 + 1.0;
    const float y = (float)(vertexIndex & 1) * -4.0 + 1.0;

    return float4(x, y, 0.0, 1.0);
}

float2 GetFullScreenTriangleUv(uint vertexId)
{
    const float u = 1.0 - (float)(vertexId >> 1) * 2.0;
    const float v = (float)(vertexId & 1) * 2.0;

    return float2(u, v);
}
