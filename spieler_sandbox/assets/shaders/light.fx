namespace Light
{

#define MAX_LIGHT_COUNT 16

    struct Light
    {
        float3  Strength;
        float   FalloffStart;       // point/spot light only
        float3  Direction;          // directional/spot light only
        float   FalloffEnd;         // point/spot light only
        float3  Position;           // point light only
        float   SpotPower;          // spot light only
    };

    struct Material
    {
        float4  DiffuseAlbedo;
        float3  FresnelR0;
        float   Shininess;
    };

    float CalcAttenuation(float distance, float falloffStart, float falloffEnd)
    {
        // Linear falloff
        return saturate((falloffEnd - distance) / (falloffEnd - falloffStart));
    }

    // Schlick gives an approximation to Fresnel reflectance
    // r0 = ((n - 1) / (n + 1)) ^ 2, where n is the index of refraction
    float3 SchlickFresnel(float3 r0, float3 normal, float3 light)
    {
        const float cosIncidentAngle = saturate(dot(normal, light));

        const float     f0              = 1.0f - cosIncidentAngle;
        const float3    reflectPercent  = r0 + (1.0f - r0) * (f0 * f0 * f0 * f0 * f0);

        return reflectPercent;
    }

    float3 CalcLightStrengthByLambertsCosineLaw(float3 lightStrength, float3 lightVector, float3 normal)
    {
        return lightStrength * max(dot(normal, lightVector), 0.0f);
    }

    float3 BlinnPhong(float3 lightStrength, float3 lightVector, float3 normal, float3 viewVector, Material material)
    {
        const float     m               = material.Shininess * 256.0f;
        const float3    halfVector      = normalize(viewVector + lightVector);
        const float     roughnessFactor = (m + 8.0f) * pow(max(dot(halfVector, normal), 0.0f), m) / 8.0f;
        const float3    fresnelFactor   = SchlickFresnel(material.FresnelR0, halfVector, lightVector);

        float3 specularAlbedo = fresnelFactor * roughnessFactor;
        specularAlbedo = specularAlbedo / (specularAlbedo + 1.0f);

        return (material.DiffuseAlbedo.rgb + specularAlbedo) * lightStrength;
    }

    // Directional light
    float3 ComputeDirectionalLight(Light light, Material material, float3 normal, float3 viewVector)
    {
        const float     lightVector     = -light.Direction;
        const float3    lightStrength   = CalcLightStrengthByLambertsCosineLaw(light.Strength, lightVector, normal);

        return BlinnPhong(lightStrength, lightVector, normal, viewVector, material);
    }

    // Point light
    float3 ComputePointLight(Light light, Material material, float3 position, float3 normal, float3 viewVector)
    {
        // The vector from the surface to the light
        float3 lightVector = light.Position - position;

        // The distance from the surface to the light
        const float distance = length(lightVector);

        if (distance > light.FalloffEnd)
        {
            return float3(0.0f, 0.0f, 0.0f);
        }

        // Normalise light vector
        lightVector /= distance;

        float3 lightStrength = CalcLightStrengthByLambertsCosineLaw(light.Strength, lightVector, normal);

        // Attenuate light by distance
        float attenuation = CalcAttenuation(distance, light.FalloffStart, light.FalloffEnd);
        lightStrength *= attenuation;

        return BlinnPhong(lightStrength, lightVector, normal, viewVector, material);
    }

    // Spot light
    float3 ComputeSpotLight(Light light, Material material, float3 position, float3 normal, float3 viewVector)
    {
        // The vector from the surface to the light
        float3 lightVector = light.Position - position;

        // The distance from the surface to the light
        const float distance = length(lightVector);

        if (distance > light.FalloffEnd)
        {
            return float3(0.0f, 0.0f, 0.0f);
        }

        // Normalise light vector
        lightVector /= distance;

        float3 lightStrength = CalcLightStrengthByLambertsCosineLaw(light.Strength, lightVector, normal);

        // Scale by spotlight
        float spotFactor = pow(max(dot(-lightVector, light.Direction), 0.0f), light.SpotPower);
        lightStrength *= spotFactor;

        return BlinnPhong(lightStrength, lightVector, normal, viewVector, material);
    }

    float4 ComputeLighting(Light lights[MAX_LIGHT_COUNT], Material material, float3 position, float3 normal, float3 viewVector, float3 shadowFactor)
    {
        float3 result = 0.0f;

#if defined(DIRECTIONAL_LIGHT_COUNT) && DIRECTIONAL_LIGHT_COUNT > 0
        for (int i = 0; i < DIRECTIONAL_LIGHT_COUNT; ++i)
        {
            result += ComputeDirectionalLight(lights[i], material, normal, viewVector);
        }
#endif

#if defined(POINT_LIGHT_COUNT) && POINT_LIGHT_COUNT > 0
        for (int i = DIRECTIONAL_LIGHT_COUNT; i < DIRECTIONAL_LIGHT_COUNT + POINT_LIGHT_COUNT; ++i)
        {
            result += ComputePointLight(lights[i], material, position, normal, viewVector);
        }
#endif

#if defined(SPOT_LIGHT_COUNT) && SPOT_LIGHT_COUNT > 0
        for (int i = POINT_LIGHT_COUNT; i < POINT_LIGHT_COUNT + SPOT_LIGHT_COUNT; ++i)
        {
            result += ComputeSpotLight(lights[i], material, position, normal, viewVector);
        }
#endif

        return float4(result, 0.0f);
    }

} // namespace Light