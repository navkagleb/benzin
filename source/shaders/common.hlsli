#pragma once

static const uint g_InvalidIndex = -1;
    
static const float g_Pi = 3.1415926535897932384626433832795;
static const float g_PiDiv2 = g_Pi / 2.0;
static const float g_TwoPi = g_Pi * 2.0;
static const float g_PiDiv180 = g_Pi / 180.0;

static const float g_Epsilon = 0.0001;
static const float g_NaN = 0.0 / 0.0;

static const float4x4 g_IdentityMatrix =
{
    { 1.0, 0.0, 0.0, 0.0 },
    { 0.0, 1.0, 0.0, 0.0 },
    { 0.0, 0.0, 1.0, 0.0 },
    { 0.0, 0.0, 0.0, 1.0 },
};

float3 LinearToGamma(float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

float4 LinearToGamma(float4 color)
{
    return pow(color, 1.0f / 2.2f);
}

template <typename T>
bool IsInRange(T value, T min, T max)
{
    return all(value >= min) && all(value <= max);
}

float DegreesToRadians(float degrees)
{
    return degrees * g_PiDiv180;
}
