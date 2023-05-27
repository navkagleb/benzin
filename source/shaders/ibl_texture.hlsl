#include "common.hlsli"

static const float PI = 3.1415926535897932384626433832795;

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

enum RootConstant : uint32_t
{
    g_InputCubeTextureIndex,
    g_OutIrradianceTextureIndex,
};

[numthreads(8, 8, 1)]
void CS_Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    TextureCube<float4> inCubeTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_InputCubeTextureIndex)];
    RWTexture2DArray<float4> outIrradianceTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_OutIrradianceTextureIndex)];

    float width;
    float height;
    float depth;
    outIrradianceTexture.GetDimensions(width, height, depth);

    float2 x = float2(dispatchThreadID.xy);
    float2 texCoord = x / float2(width, height);

    texCoord.y = 1.0 - texCoord.y;
    texCoord *= 2.0;
    texCoord -= 1.0;

    uint face = dispatchThreadID.z;

    float3 sampleDir = 0.0f;
    switch (face)
    {
        case 0: //pos x
            sampleDir = float3(1.0, texCoord.y, texCoord.x);
            break;
        case 1: //neg x
            sampleDir = float3(-1.0, texCoord.y, -texCoord.x);
            break;
        case 2: //pos y
            sampleDir = float3(texCoord.x, 1.0, texCoord.y);
            break;
        case 3: //neg y
            sampleDir = float3(texCoord.x, -1.0, -texCoord.y);
            break;
        case 4: //pos z
            sampleDir = float3(texCoord.x, texCoord.y, -1.0);
            break;
        case 5: //neg z
            sampleDir = float3(-texCoord.x, texCoord.y, 1.0);
            break;
    }

    float3 N = normalize(float3(sampleDir.xy, -sampleDir.z));
    float3 R = N;
    float3 V = R;

    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = cross(up, N);
    up = cross(N, right);

    float3 irradiance = 0.0;
    const uint SAMPLE_COUNT = 16384u;
    const float roughness = 1.0;
    float totalWeight = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, 1.0);

        float NdotH = max(dot(N, H), 0.0);
        float D = 1.0 / PI;
        float pdf = (D * NdotH / (4.0)) + 0.0001;

        //float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
        //float saTexel = 4.0 * PI / (6.0 * resolution * resolution);

        //float mipLevel = 0.5 * log2(saSample / saTexel);

        irradiance += inCubeTexture.SampleLevel(common::g_LinearWrapSampler, H, 0.0f).rgb * NdotH;

        //irradiance += textureLod(environmentMap, H, mipLevel).rgb * NdotH;
        totalWeight += NdotH;
    }

    irradiance = PI * irradiance / totalWeight;
    outIrradianceTexture[dispatchThreadID] = float4(irradiance, 1.0f);
}