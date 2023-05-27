#include "common.hlsli"
#include "fullscreen_helper.hlsli"
#include "gbuffer_utils.hlsli"

namespace internal
{

    static const float g_PI = 3.14159265359;

    struct DirectionalLight
    {
        float3 Color;
        float Intensity;
        float3 WorldDirection;
    };

    struct Attenuation
    {
        float Constant;
        float Linear;
        float Exponential;
    };

    struct PointLight
    {
        float3 Color;
        float Intensity;
        float3 WorldPosition;
        Attenuation Attenuation;
    };

    struct Light
    {
        float3 Color;
        float Intensity;
        float3 Direction;
    };

    float3 UsePBRMaterialModel()
    {
        // Bidirectional
        // Reflectance
        // Distribution
        // Function
        // BRDF = kD * fDiffuse + kS * fSpecular
        // kD + kS = 1.0f

        // kS = Fresnel
        // KD = 1.0f - kS

        // F_Shlink = f0 + (1.0f - f0) * (1.0f - V * H) ^ 5
        // f0 - base reflectivity
        // V - view vector
        // H - half-way vector

        // -- Diffuse
        // Lambertian Model
        // Oren-Nayar Model - more realistic

        // -- Lambertian Model
        // f_Lambert = (Color / PI) * (L_n * N)
        // Color - color of the object
        // L_n - incoming light
        // N - normal

        // -- Specular
        // Cook-Torrance Model or Torrance-Sparrow Model
        // f_Cook-Torrance = DGF / ((V * N) * (L * N))
        // f_Cook-Torrance = DGF / (4 * (V * N) * (L * N)) - correct version
        // f_Cook-Torrance = DGF / (PI * (V * N) * (L * N))
        // D - Normal Distribution Function
        // G - Geometry Shadowing Function
        // F - Fresnel Function
        // BRDF = k_d * f_diffuse + <no k_s> * f_specular

        // -- D - Normal Distribution Function
        // Beckmann Model
        // GGX / Trowbridge-Reitz
        // GGX / Anisotropic

        // -- GGX / Trowbridge-Reitz
        // D_GGX/Trowbridge-Reitz = (A ^ 2) / (PI * ((N * H) ^ 2 * (A ^ 2 - 1) + 1) ^ 2
        // A - roughness ^ 2

        // G - Geometry Shadowing Function
        // Neumann Model
        // Kelemen Model
        // Smith Model
        // Schlick-Beckmann Model
        // Smith + Schlick-Beckmann = Schlick-GGX

        // G_Smith = G1(L, N) * G1(V, N)
        // G_Schlick-Beckmann = (N * X) / ((N * X)(1 - k) + k)
        // k = A / 2
        // k = (roughness + 1) ^ 2 / 8 - Disney and UnrealEngine in the past
        // X = V or L

        return 0.0;
    }

    namespace pbr
    {

        struct Material
        {
            float4 Albedo;
            float3 Emissive;
            float Metalness;
            float3 F0;
            float Roughness;
        };

        float3 NormalDistributionFunction(float3 normal, float3 halfDirection, float alpha)
        {
            // GGX / Trowbridge - Reitz

            const float alpha2 = alpha * alpha;

            const float3 nDotH = max(dot(normal, halfDirection), 0.0f);
            const float3 nDotH2 = nDotH * nDotH;

            const float numerator = alpha2;
            const float3 denominator = g_PI * pow(nDotH2 * (alpha2 - 1.0f) + 1.0f, 2.0f);

            return numerator / denominator;
        }

        float ShlickBeckmann(float3 normal, float3 x, float alpha)
        {
            // GeometryShadowingFunction

            const float k = alpha / 2.0f;

            const float normalDotX = max(dot(normal, x), 0.0f);

            const float numerator = normalDotX;
            const float denominator = normalDotX * (1.0f - k) + k;

            return numerator / max(denominator, 0.000001f);
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

            const float cosTheta = max(dot(viewDirection, halfDirection), 0.0f);
            const float f = pow(1.0f - cosTheta, 5.0f);

            return f0 + (1.0f - f0) * f;
        }

        float3 DiffuseFunction(float3 albedo)
        {
            // Lambertian Model
            // dot(lightDirection, normal) included in overall formula

            return albedo / g_PI;
        }

        float3 SpecularFunction(Light light, Material material, float3 viewDirection, float3 normal, float3 halfDirection)
        {
            // Cook-Torrance

            const float alpha = material.Roughness * material.Roughness;

            const float3 d = NormalDistributionFunction(normal, halfDirection, alpha);
            const float g = GeometryShadowingFunction(normal, viewDirection, light.Direction, alpha);
            const float3 f = FresnelFunction(material.F0, viewDirection, halfDirection);

            const float3 vDotN = max(dot(viewDirection, normal), 0.0f);
            const float3 lDotN = max(dot(light.Direction, normal), 0.0f);

            const float3 numerator = d * g * f;
            const float3 denominator = 4.0f * vDotN * lDotN;

            return numerator / max(denominator, 0.000001f);
        }

        float3 BidirectionalReflectanceDistributionFunction(Light light, Material material, float3 viewDirection, float3 normal)
        {
            const float3 halfDirection = normalize(light.Direction + viewDirection);

            const float3 kS = FresnelFunction(material.F0, viewDirection, halfDirection); // Specular Factor
            const float3 kD = (1.0f - kS) * (1.0f - material.Metalness); // Diffuse Factor

            const float3 diffuseFunction = DiffuseFunction(material.Albedo.rgb);
            const float3 specularFunction = SpecularFunction(light, material, viewDirection, normal, halfDirection);

            return kD * diffuseFunction + specularFunction; // kS included in specularFunction
        }

        float3 Apply(Light light, Material material, float3 viewDirection, float3 normal)
        {
            const float3 brdf = BidirectionalReflectanceDistributionFunction(light, material, viewDirection, normal);
            const float3 lDotN = max(dot(light.Direction, normal), 0.0f);

            return (brdf * light.Color * lDotN) * light.Intensity;
        }

    } // namespace pbr

    float CalculateAttenuation(float distance, Attenuation attenuation)
    {
        return 1.0f / (attenuation.Constant + attenuation.Linear * distance + attenuation.Exponential * distance * distance);
    }

    float3 ComputeDirectionalLightImpact(DirectionalLight directionalLight, pbr::Material material, float3 worldViewDirection, float3 worldNormal)
    {
        Light light;
        light.Color = directionalLight.Color;
        light.Intensity = directionalLight.Intensity;
        light.Direction = normalize(-directionalLight.WorldDirection);

        return pbr::Apply(light, material, worldViewDirection, worldNormal);
    }

    float3 ComputePointLightImpact(PointLight pointLight, pbr::Material material, float3 worldPosition, float3 worldViewDirection, float3 worldNormal)
    {
        float3 lightDirection = pointLight.WorldPosition - worldPosition;
        const float distance = length(lightDirection);

        lightDirection /= distance;

        Light light;
        light.Color = pointLight.Color;
        light.Intensity = pointLight.Intensity;
        light.Direction = lightDirection;

        const float attenuation = CalculateAttenuation(distance, pointLight.Attenuation);
        const float3 pbr = pbr::Apply(light, material, worldViewDirection, worldNormal);

        return attenuation * pbr;
    }

} // namespace internal

enum : uint32_t
{
    g_PassBufferIndex,

    g_AlbedoTextureIndex,
    g_WorldNormalTextureIndex,
    g_EmissiveTextureIndex,
    g_RoughnessMetalnessTextureIndex,
    g_DepthTextureIndex,

    g_PointLightBufferIndex,

    g_OutputType,
};

enum OutputType : uint32_t
{
    Final,
    ReconsructedWorldPosition,
    GBuffer_Depth,
    GBuffer_Albedo,
    GBuffer_WorldNormal,
    GBuffer_Emissive,
    GBuffer_Roughness,
    GBuffer_Metalness,
};

struct PassData
{
    float4x4 InverseViewMatrix;
    float4x4 InverseProjectionMatrix;
    float3 WorldCameraPosition;
    uint32_t PointLightCount;
    float3 SunColor;
    float SunIntensity;
    float3 SunDirection;
};

gbuffer::Unpacked ReadGBuffer(float2 uv)
{
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_AlbedoTextureIndex)];
    Texture2D<float4> worldNormal = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_WorldNormalTextureIndex)];
    Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_EmissiveTextureIndex)];
    Texture2D<float4> roughnessMetalnessTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_RoughnessMetalnessTextureIndex)];

    gbuffer::Packed packed;
    packed.Color0 = albedoTexture.Sample(common::g_PointWrapSampler, uv);
    packed.Color1 = worldNormal.Sample(common::g_PointWrapSampler, uv);
    packed.Color2 = emissiveTexture.Sample(common::g_PointWrapSampler, uv);
    packed.Color3 = roughnessMetalnessTexture.Sample(common::g_PointWrapSampler, uv);

    return gbuffer::Unpack(packed);
}

float ReadDepth(float2 uv)
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_DepthTextureIndex)];

    return depthTexture.Sample(common::g_PointWrapSampler, uv).r;
}

float4 PS_Main(fullscreen_helper::VS_Output input) : SV_Target
{
    ConstantBuffer<PassData> passData = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_PassBufferIndex)];
    StructuredBuffer<internal::PointLight> pointLightBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_PointLightBufferIndex)];

    const uint32_t outputType = BENZIN_GET_ROOT_CONSTANT(g_OutputType);

    const gbuffer::Unpacked gbuffer = ReadGBuffer(input.UV);
    const float depth = ReadDepth(input.UV);

    const float3 worldPosition = gbuffer::ReconstructWorldPositionFromDepth(input.UV, depth, passData.InverseProjectionMatrix, passData.InverseViewMatrix);

    if (depth == 1.0f)
    {
        discard;
    }

    switch (outputType)
    {
        case OutputType::ReconsructedWorldPosition:
        {
            return float4(worldPosition, 1.0f);
        }
        case OutputType::GBuffer_Depth:
        {
            // TODO: 
            const float nearZ = 0.1f;
            const float farZ = 100.0f;
            const float linearDepth = (2.0 * nearZ) / (farZ + nearZ - depth * (farZ - nearZ));
            return float4(linearDepth.xxx, 1.0f);
        }
        case OutputType::GBuffer_Albedo:
        {
            return gbuffer.Albedo;
        }
        case OutputType::GBuffer_WorldNormal:
        {
            return float4(gbuffer.WorldNormal, 1.0f);
        }
        case OutputType::GBuffer_Emissive:
        {
            return float4(gbuffer.Emissive, 1.0f);
        }
        case OutputType::GBuffer_Roughness:
        {
            return float4(gbuffer.Roughness.xxx, 1.0f);
        }
        case OutputType::GBuffer_Metalness:
        {
            return float4(gbuffer.Metalness.xxx, 1.0f);
        }
        default:
        {
            break;
        }
    }

    internal::DirectionalLight sunLight;
    sunLight.Color = passData.SunColor;
    sunLight.Intensity = passData.SunIntensity;
    sunLight.WorldDirection = passData.SunDirection;

    float3 fresnelR0 = float3(0.04f, 0.04f, 0.04f);
    fresnelR0 = lerp(fresnelR0, gbuffer.Albedo.rgb, gbuffer.Metalness);

    internal::pbr::Material material;
    material.Albedo = gbuffer.Albedo;
    material.Emissive = gbuffer.Emissive;
    material.Roughness = gbuffer.Roughness;
    material.Metalness = gbuffer.Metalness;
    material.F0 = fresnelR0;

    const float3 worldViewDirection = normalize(passData.WorldCameraPosition - worldPosition);
    const float3 ambient = 0.1f * gbuffer.Albedo.rgb;

    float3 color = internal::ComputeDirectionalLightImpact(sunLight, material, worldViewDirection, gbuffer.WorldNormal);

    for (uint32_t i = 0; i < passData.PointLightCount; ++i)
    {
        color += internal::ComputePointLightImpact(pointLightBuffer[i], material, worldPosition, worldViewDirection, gbuffer.WorldNormal);
    }

    color = color + ambient + material.Emissive;
    color = color / (color + 1.0f);
    return float4(color, 1.0f);
}
