#pragma once

float RgbToLuminance(float3 rgb)
{
    return dot(rgb, float3(0.2125, 0.7154, 0.0721));
}

float3 LinearToSrgb(float3 rgb)
{
    return pow(rgb, 1.0 / 2.2);
}

float3 SrgbToLinear(float3 srgb)
{
    return pow(srgb, 2.2);
}

float3 LinearToSrgbAccurate(float3 rgb)
{
    const float3 srgbLo = rgb * 12.92;
    const float3 srgbHi = pow(abs(rgb), 1.0 / 2.4) * 1.055 - 0.055;

    return lerp(srgbHi, srgbLo, rgb <= 0.0031308);
}

float3 SrgbToLinearAccurate(float3 srgb)
{
    const float3 rgbLo = srgb / 12.92;
    const float3 rgbHi = pow((srgb + 0.055) / 1.055, 2.4);

    return lerp(rgbHi, rgbLo, srgb <= 0.04045);
}
