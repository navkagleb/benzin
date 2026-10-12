#pragma once // TODO: Does it work?

static const uint g_MaxU32 = -1;

static const float g_Pi = 3.1415926535897932384626433832795;
static const float g_PiDiv2 = g_Pi / 2.0;
static const float g_TwoPi = g_Pi * 2.0;
static const float g_PiDiv180 = g_Pi / 180.0;

static const float g_Epsilon = 0.0001;
static const float g_NaN = 0.0 / 0.0;

static const float3 g_UpDir = float3(0.0, 1.0, 0.0);

uint DivideUp(uint value, uint divisor)
{
    return (value + divisor - 1) / divisor;
}

uint RoundUp(uint value, uint divisor)
{
    return DivideUp(value, divisor) * divisor;
}

template <typename T>
bool IsInRange(T value, T min, T max)
{
    return all(value >= min) && all(value <= max);
}

float4 GetFullScreenTriangleClipPosition(uint vertexIndex)
{
    const float x = (float)(vertexIndex >> 1) * -4.0 + 1.0;
    const float y = (float)(vertexIndex & 1) * -4.0 + 1.0;

    return float4(x, y, 0.0, 1.0);
}

float2 GetFullScreenTriangleUv(uint vertexIndex)
{
    const float u = 1.0 - (float)(vertexIndex >> 1) * 2.0;
    const float v = (float)(vertexIndex & 1) * 2.0;

    return float2(u, v);
}
