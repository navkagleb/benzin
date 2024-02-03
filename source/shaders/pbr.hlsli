#pragma once

#include "common.hlsli"

// Ref: https://learnopengl.com/PBR/Theory

struct PBRLight
{
    float3 Color;
    float Intensity;
    float3 Direction;
};

struct PBRMaterial
{
    float4 Albedo;
    float Metalness;
    float3 F0;
    float Roughness;
};

float3 GetF0(float3 albedo, float metalness)
{
    const float3 fresnelR0 = 0.04f;
    return lerp(fresnelR0, albedo, metalness);
}

float3 NormalDistributionFunction(float3 normal, float3 halfDirection, float alpha)
{
    // GGX / Trowbridge - Reitz

    const float alpha2 = alpha * alpha;

    const float3 nDotH = max(0.0f, dot(normal, halfDirection));
    const float3 nDotH2 = nDotH * nDotH;

    const float numerator = alpha2;
    const float3 denominator = g_PI * pow(nDotH2 * (alpha2 - 1.0f) + 1.0f, 2.0f);

    return numerator / denominator;
}

float ShlickBeckmann(float3 normal, float3 x, float alpha)
{
    // GeometryShadowingFunction

    const float k = alpha / 2.0f;

    const float normalDotX = max(0.0f, dot(normal, x));

    const float numerator = normalDotX;
    const float denominator = normalDotX * (1.0f - k) + k;

    return numerator / max(g_FloatEpsilon, denominator);
}

float GeometryShadowingFunction(float3 normal, float3 viewDirection, float3 lightDirection, float alpha)
{
    // Smith + ShlickBeckmann Model for GeomtryShadowingFunction

    const float arg1 = ShlickBeckmann(normal, viewDirection, alpha);
    const float arg2 = ShlickBeckmann(normal, lightDirection, alpha);

    return arg1 * arg2;
}

float3 FresnelFunction(float3 f0, float3 viewDirection, float3 halfDirection)
{
    // Fresnel-Schlick Function

    const float cosTheta = max(0.0f, dot(viewDirection, halfDirection));
    const float f = pow(1.0f - cosTheta, 5.0f);

    return f0 + (1.0f - f0) * f;
}

float3 DiffuseFunction(float3 albedo)
{
    // Lambertian Model
    // dot(lightDirection, normal) included in overall formula

    return albedo / g_PI;
}

float3 SpecularFunction(PBRLight light, PBRMaterial material, float3 viewDirection, float3 normal, float3 halfDirection)
{
    // Cook-Torrance

    const float alpha = material.Roughness * material.Roughness;

    const float3 d = NormalDistributionFunction(normal, halfDirection, alpha);
    const float g = GeometryShadowingFunction(normal, viewDirection, light.Direction, alpha);
    const float3 f = FresnelFunction(material.F0, viewDirection, halfDirection);

    const float3 vDotN = max(0.0f, dot(viewDirection, normal));
    const float3 lDotN = max(0.0f, dot(light.Direction, normal));

    const float3 numerator = d * g * f;
    const float3 denominator = 4.0f * vDotN * lDotN;

    return numerator / max(g_FloatEpsilon, denominator);
}

float3 BidirectionalReflectanceDistributionFunction(PBRLight light, PBRMaterial material, float3 viewDirection, float3 normal)
{
    const float3 halfDirection = normalize(light.Direction + viewDirection);

    const float3 kS = FresnelFunction(material.F0, viewDirection, halfDirection); // Specular Factor
    const float3 kD = (1.0f - kS) * (1.0f - material.Metalness); // Diffuse Factor

    const float3 diffuseFunction = DiffuseFunction(material.Albedo.rgb);
    const float3 specularFunction = SpecularFunction(light, material, viewDirection, normal, halfDirection);

    return kD * diffuseFunction + specularFunction; // kS included in specularFunction
}

float3 GetPBRLitColor(PBRLight light, PBRMaterial material, float3 viewDirection, float3 normal)
{
    const float3 brdf = BidirectionalReflectanceDistributionFunction(light, material, viewDirection, normal);
    const float3 lDotN = max(0.0f, dot(light.Direction, normal));

    return (brdf * light.Color * lDotN) * light.Intensity;
}
