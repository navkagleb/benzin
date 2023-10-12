#pragma once

namespace fullscreen_helper
{

    struct VS_Output
    {
        float4 HomogeneousPosition : SV_Position;
        float4 ClipPosition : ClipPosition;
        float2 UV : UV;
    };

    float4 GetTriangleHomogeneousPosition(uint32_t vertexID)
    {
        const float x = (float)(vertexID >> 1) * -4.0f + 1.0f;
        const float y = (float)(vertexID & 1) * -4.0f + 1.0f;

        return float4(x, y, 0.0f, 1.0f);
    }

    float2 GetTriangleUV(uint32_t vertexID)
    {
        const float u = 1.0f - (float)(vertexID >> 1) * 2.0f;
        const float v = (float)(vertexID & 1) * 2.0f;

        return float2(u, v);
    }

} // namespace fullscreen_helper
