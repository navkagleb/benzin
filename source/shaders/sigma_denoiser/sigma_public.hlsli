#pragma once

namespace sigma
{

    static const float g_Fp16Max = 65504.0;
    static const float g_Eps = 1e-6;

    float ClampPenumbra(float distanceToOccluder, float penumbraRadius)
    {
        return distanceToOccluder >= g_Fp16Max ? g_Fp16Max : min(penumbraRadius, 32768.0);
    }

    float PackPenumbra(float distanceToOccluder, float tanOfLightAngularRadius)
    {
        // Infinite (directional) light source
        // Returns the size of penumbra in world space

        const float penumbraRadius = distanceToOccluder * tanOfLightAngularRadius; // Light size from occluder point of view

        return ClampPenumbra(distanceToOccluder, penumbraRadius);
    }

    float PackPenumbra(float distanceToOccluder, float distanceToLight, float lightRadius)
    {
        // Local light source
        // "lightSize" must be an acceptable projection to the plane perpendicular to the light direction

        const float tanOfLightAngularRadius = lightRadius / max(distanceToLight - distanceToOccluder, g_Eps);
        const float penumbraRadius = distanceToOccluder * tanOfLightAngularRadius;

        return ClampPenumbra(distanceToOccluder, penumbraRadius);
    }

    float PackShadow(float shadow)
    {
        return sqrt(saturate(shadow));
    }

    float UnpackShadow(float shadow)
    {
        return shadow * shadow;
    }

    bool IsLit(float penumbra)
    {
        return penumbra >= sigma::g_Fp16Max;
    }

}