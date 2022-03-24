namespace Light
{

#define MAX_LIGHT_COUNT 16

    struct Light
    {
        float3 Strength;
        float FalloffStart;     // Point / Spot
        float3 Direction;       // Directional / Spot
        float FalloffEnd;       // Point / Spot
        float3 Position;        // Point / Spot
        float SpotPower;        // Spot
    };

    typedef Light LightContainer[MAX_LIGHT_COUNT];

    struct Material
    {
        float4 DiffuseAlbedo;
        float3 FresnelR0;
        float Shininess;
    };

    float CalcAttenuation(float distance, float falloffStart, float falloffEnd)
    {
#if 1
        // Linear falloff
        return saturate((falloffEnd - distance) / (falloffEnd - falloffStart));
#else
        const float att0 = 0.4f;
        const float att1 = 0.02f;
        const float att2 = 0.0f;

        return 1.0f / (att0 + att1 * distance + att2 * (distance * distance));
#endif
    }

    // Schlick gives an approximation to Fresnel reflectance
    // r0 = ((n - 1) / (n + 1)) ^ 2, where n is the index of refraction
    float3 SchlickFresnel(float3 r0, float3 normal, float3 lightVector)
    {
        const float cosIncidentAngle = saturate(dot(normal, lightVector));
        const float f0 = 1.0f - cosIncidentAngle;
        const float3 reflectPercent = r0 + (1.0f - r0) * (f0 * f0 * f0 * f0 * f0);

        return reflectPercent;
    }

    float3 CalcLightStrengthByLambertsCosineLaw(float3 lightStrength, float3 lightVector, float3 normal)
    {
        return lightStrength * max(dot(lightVector, normal), 0.0f);
    }

    float3 BlinnPhong(float3 lightStrength, float3 lightVector, float3 normal, float3 viewVector, Material material)
    {
        const float m = material.Shininess * 256.0f;
        const float3 halfVector = normalize(viewVector + lightVector);
        const float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVector, normal), 0.0f), m) / 8.0f;
        const float3 fresnelFactor = SchlickFresnel(material.FresnelR0, halfVector, lightVector);

        float3 specularAlbedo = fresnelFactor * roughnessFactor;
        specularAlbedo = specularAlbedo / (specularAlbedo + 1.0f);

        return (material.DiffuseAlbedo.rgb + specularAlbedo) * lightStrength;
    }

    // Directional light
    float3 ComputeDirectionalLight(Light light, Material material, float3 normal, float3 viewVector)
    {
        const float3 lightVector = -light.Direction;
        const float3 lightStrength = CalcLightStrengthByLambertsCosineLaw(light.Strength, lightVector, normal);

        return BlinnPhong(lightStrength, lightVector, normal, viewVector, material);
    }

    // Point light
    float3 ComputePointLight(Light light, Material material, float3 position, float3 normal, float3 viewVector)
    {
        float3 result = 0.0f;

        // The vector from the surface to the light
        float3 lightVector = light.Position - position;

        // The distance from the surface to the light
        const float distance = length(lightVector);

        if (distance <= light.FalloffEnd)
        {
            // Normalise light vector
            lightVector /= distance;

            float3 lightStrength = CalcLightStrengthByLambertsCosineLaw(light.Strength, lightVector, normal);

            // Attenuate light by distance
            float attenuation = CalcAttenuation(distance, light.FalloffStart, light.FalloffEnd);
            lightStrength *= attenuation;

            result = BlinnPhong(lightStrength, lightVector, normal, viewVector, material);
        }

        return result;
    }

    // Spot light
    float3 ComputeSpotLight(Light light, Material material, float3 position, float3 normal, float3 viewVector)
    {
        float3 result = 0.0f;

        // The vector from the surface to the light
        float3 lightVector = light.Position - position;

        // The distance from the surface to the light
        const float distance = length(lightVector);

        if (distance < light.FalloffEnd)
        {
            // Normalise light vector
            lightVector /= distance;

            float3 lightStrength = CalcLightStrengthByLambertsCosineLaw(light.Strength, lightVector, normal);

            // Attenuate light by distance.
            float attenuation = CalcAttenuation(distance, light.FalloffStart, light.FalloffEnd) + 0.2f;
            lightStrength *= attenuation;

            // Scale by spotlight
            float spotFactor = pow(max(dot(-lightVector, light.Direction), 0.0f), light.SpotPower);
            lightStrength *= spotFactor;

            // result = lightStrength;
            result = BlinnPhong(lightStrength, lightVector, normal, viewVector, material);
        }

        return result;
    }

    float4 ComputeLighting(LightContainer lights, Material material, float3 position, float3 normal, float3 viewVector, float3 shadowFactor)
    {
        float3 result = 0.0f;
        uint i = 0;

#if defined(DIRECTIONAL_LIGHT_COUNT)
#if DIRECTIONAL_LIGHT_COUNT > 1
        for (; i < DIRECTIONAL_LIGHT_COUNT; ++i)
        {
            result += shadowFactor[i] * ComputeDirectionalLight(lights[i], material, normal, viewVector);
        }
#else
        result += shadowFactor[i] * ComputeDirectionalLight(lights[0], material, normal, viewVector);
#endif // DIRECTIONAL_LIGHT_COUNT > 1
#endif // defined(DIRECTIONAL_LIGHT_COUNT)

#if defined(POINT_LIGHT_COUNT)
#if POINT_LIGHT_COUNT > 1
        for (i = DIRECTIONAL_LIGHT_COUNT; i < DIRECTIONAL_LIGHT_COUNT + POINT_LIGHT_COUNT; ++i)
        {
            result += ComputePointLight(lights[i], material, position, normal, viewVector);
        }
#else
        result += ComputePointLight(lights[DIRECTIONAL_LIGHT_COUNT], material, position, normal, viewVector);
#endif // POINT_LIGHT_COUNT > 1
#endif // defined(POINT_LIGHT_COUNT)

#if defined(SPOT_LIGHT_COUNT)
#if SPOT_LIGHT_COUNT > 1
        for (i = DIRECTIONAL_LIGHT_COUNT + POINT_LIGHT_COUNT; i < DIRECTIONAL_LIGHT_COUNT + POINT_LIGHT_COUNT + SPOT_LIGHT_COUNT; ++i)
        {
            result += ComputeSpotLight(lights[i], material, position, normal, viewVector);
        }
#else
        result += ComputeSpotLight(lights[DIRECTIONAL_LIGHT_COUNT + POINT_LIGHT_COUNT], material, position, normal, viewVector);
#endif // SPOT_LIGHT_COUNT > 1
#endif // defined(SPOT_LIGHT_COUNT)

        return float4(result, 0.0f);
    }

} // namespace Light