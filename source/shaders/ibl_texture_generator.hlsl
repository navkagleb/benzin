#include "common.hlsli"
#include "cube_helper.hlsli"

//namespace
//{

float3x3 GetDummyTBNBasis(float3 normal)
{
    const float3 dummyUp = float3(0.0f, 1.0f, 0.0f);

    const float3 tangent = normalize(cross(dummyUp, normal)); // right
    const float3 bitangent = normalize(cross(normal, tangent)); // up

    return float3x3(tangent, bitangent, normal);
}

static const uint SAMPLES = 16384;
static const float INV_SAMPLES = 1.0f / (float)SAMPLES;

float vanDerCorputRadicalInverse(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10;
}

float2 sampleHammersleySequence(uint i)
{
    return float2((float)i * INV_SAMPLES, vanDerCorputRadicalInverse(i));
}

float3 uniformSampleHemisphere(float2 uv)
{
    const float z = uv.x;
    const float r = pow(max(0.0f, 1.0f - z * z), 0.5f);
    const float phi = 2.0f * common::g_PI * uv.y;
    return float3(r * cos(phi), r * sin(phi), z);
}

void computeBasisVectors(float3 normal, out float3 s, out float3 t)
{
    t = cross(normal, float3(0.0f, 1.0f, 0.0f));
    t = lerp(cross(normal, float3(1.0f, 0.0f, 0.0f)), t, step(0.00001f, dot(t, t)));

    t = normalize(t);
    s = normalize(cross(normal, t));
}

float3 tangentToWorldCoords(float3 v, float3 n, float3 s, float3 t)
{
    return s * v.x + t * v.y + n * v.z;
}

float3 getSamplingVector(float2 pixelCoords, uint3 dispatchThreadID)
{
    // Convert pixelCoords into the range of -1 .. 1 and make sure y goes from top to bottom.
    float2 uv = 2.0f * float2(pixelCoords.x, 1.0f - pixelCoords.y) - float2(1.0f, 1.0f);

    float3 samplingVector = float3(0.0f, 0.0f, 0.0f);

    // Based on cube face 'index', choose a vector.
    switch (dispatchThreadID.z)
    {
        case 0:
            samplingVector = float3(1.0, uv.y, -uv.x);
            break;
        case 1:
            samplingVector = float3(-1.0, uv.y, uv.x);
            break;
        case 2:
            samplingVector = float3(uv.x, 1.0, -uv.y);
            break;
        case 3:
            samplingVector = float3(uv.x, -1.0, uv.y);
            break;
        case 4:
            samplingVector = float3(uv.x, uv.y, 1.0);
            break;
        case 5:
            samplingVector = float3(-uv.x, uv.y, -1.0);
            break;
    }

    return normalize(samplingVector);
}

//} // namespace internal

enum RootConstant : uint32_t
{
    g_InputCubeTextureIndex,
    g_OutIrradianceTextureIndex,
};

#if 0
[numthreads(8, 8, 1)]
void CS_GenerateIrradianceTexture(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    TextureCube<float4> inCubeTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_InputCubeTextureIndex)];
    RWTexture2DArray<float4> outIrradianceTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_OutIrradianceTextureIndex)];

    float width;
    float height;
    float depth;
    outIrradianceTexture.GetDimensions(width, height, depth);

    const float2 faceUV = (dispatchThreadID.xy + 0.5f) / float2(width, height);
    const float3 cubeUV = cube_helper::GetCubeUV(faceUV, dispatchThreadID.z);

    const float2 pixelCoords = (dispatchThreadID.xy + 0.5f) / float2(width, height);
    const float3 samplingVector = internal::getSamplingVector(pixelCoords, dispatchThreadID);

    const float3 normal = normalize(samplingVector);

    const float3x3 dummyTBNBasis = internal::GetDummyTBNBasis(normal);

    const float sampleDelta = 0.025f;
    uint32_t sampleCount = 0;

    float3 irradiance = 0.0f;

    float3 t = float3(0.0f, 0.0f, 0.0f);
    float3 s = float3(0.0f, 0.0f, 0.0f);
    internal::computeBasisVectors(normal, s, t);


    const float4 color = inCubeTexture.Sample(common::g_Anisotropic16WrapSampler, normal);

#if 0

    for (float phi = 0.0f; phi < 2.0 * common::g_PI; phi += sampleDelta)
    {
        for (float theta = 0.0f; theta < 0.5f * common::g_PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            const float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            //const float3 sampleVec = mul(tangentSample, dummyTBNBasis);
            const float3 sampleVec = mul(tangentSample, dummyTBNBasis);

            irradiance += inCubeTexture.Sample(common::g_LinearWrapSampler, sampleVec).rgb * cos(theta) * sin(theta);
            sampleCount += 1;
        }
    }

#else

    for (uint i = 0; i < internal::SAMPLES; i++)
    {
        // Get a uniform random variable which will be used to get a point on the hemisphere.

        const float2 uv = internal::sampleHammersleySequence(i);

        const float3 li = internal::tangentToWorldCoords(internal::uniformSampleHemisphere(uv), normal, s, t);
        //const float3 li = mul(internal::uniformSampleHemisphere(uv), dummyTBNBasis);

        const float cosTheta = saturate(dot(li, normal));

        irradiance += inCubeTexture.SampleLevel(common::g_LinearClampSampler, li, 0u).rgb * cosTheta;
    }

#endif

#if 0
    outIrradianceTexture[dispatchThreadID] = float4(common::g_PI * irradiance * (1.0f / sampleCount), 1.0f);
#else
    outIrradianceTexture[dispatchThreadID] = float4(irradiance * 2.0f * internal::INV_SAMPLES, 1.0f);
#endif

    //outIrradianceTexture[dispatchThreadID] = float4(normal, 1.0f);

}
#endif

[numthreads(8, 8, 1)]
void CsMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    TextureCube<float4> cubeMapTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_InputCubeTextureIndex)];

    RWTexture2DArray<float4> outputIrradianceMap = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_OutIrradianceTextureIndex)];

    float textureWidth, textureHeight, textureDepth;
    outputIrradianceMap.GetDimensions(textureWidth, textureHeight, textureDepth);

    // For each of the 6 cube faces, there will be threads which have the same 2d pixel coord. 
    const float2 pixelCoords = (dispatchThreadID.xy + 0.5f) / float2(textureWidth, textureHeight);

    // Based on current texture pixel coord (and the dispatch's z parameter : or the number of groupz on the z axis
    // (hardcoded to 6), it will calculate the normalized samling direction which we use to sample into the cube map.
    // Reference : https://github.com/Nadrin/PBR/blob/master/data/shaders/hlsl/irmap.hlsl
    const float3 samplingVector = getSamplingVector(pixelCoords, dispatchThreadID);

    // Calculation of basis vectors for converting a vector from Shading / Tangent space to world space.
    const float3 normal = normalize(samplingVector);
    float3 t = float3(0.0f, 0.0f, 0.0f);
    float3 s = float3(0.0f, 0.0f, 0.0f);

    computeBasisVectors(normal, s, t);

    // Using Monte Carlo integration to find irradiance of the hemisphere.
    // The final result should be 1 / N * summation (f(x) / PDF). PDF for sampling uniformly from hemisphere is 1 /
    // TWO_PI. 
    float3 irradiance = float3(0.0f, 0.0f, 0.0f);

    // This loop, will for each sampling direction (i.e the surface normal) will generate random points on the
    // hemisphere at p, get its world space coordinate such that it is centered at the particular pixel / point, and
    // convolute the input cube map to average the radiance, so as to get the resultant irradiance from a particular wi
    // direction.
    for (uint i = 0; i < SAMPLES; i++)
    {
        // Get a uniform random variable which will be used to get a point on the hemisphere.

        const float2 uv = sampleHammersleySequence(i);

        const float3 li = tangentToWorldCoords(uniformSampleHemisphere(uv), normal, s, t);

        const float cosTheta = saturate(dot(li, normal));

        irradiance += cubeMapTexture.SampleLevel(common::g_LinearClampSampler, li, 0u).rgb * cosTheta;
    }

    // PI cancels out as lambertian diffuse divides by PI.
    outputIrradianceMap[dispatchThreadID] = float4(irradiance * 2.0f * INV_SAMPLES, 1.0f);
}
