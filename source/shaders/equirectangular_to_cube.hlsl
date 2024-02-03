#include "common.hlsli"

float3 GetCubeUV(float2 faceUV, uint32_t faceIndex)
{
    const float2 transformedFaceUV = 2.0f * float2(faceUV.x, 1.0f - faceUV.y) - 1.0f;

    float3 cubeUV = 0.0f;
    switch (faceIndex)
    {
        case 0:
        {
            cubeUV = float3(1.0, transformedFaceUV.y, -transformedFaceUV.x);
            break;
        }
        case 1:
        {
            cubeUV = float3(-1.0, transformedFaceUV.y, transformedFaceUV.x);
            break;
        }
        case 2:
        {
            cubeUV = float3(transformedFaceUV.x, 1.0, -transformedFaceUV.y);
            break;
        }
        case 3:
        {
            cubeUV = float3(transformedFaceUV.x, -1.0, transformedFaceUV.y);
            break;
        }
        case 4:
        {
            cubeUV = float3(transformedFaceUV.x, transformedFaceUV.y, 1.0);
            break;
        }
        case 5:
        {
            cubeUV = float3(-transformedFaceUV.x, transformedFaceUV.y, -1.0);
            break;
        }
    }

    return cubeUV;
}

float2 ConvertUnitCartesianToSpherical(float3 cartesian)
{
    const float phi = atan2(cartesian.z, cartesian.x); // Azimuthal angle
    const float theta = acos(cartesian.y); // Polar angle

    return float2(phi, theta);
}

float2 ConvertSphericalToUV(float phi, float theta)
{
    const float u = 0.5f - phi / g_2PI;
    const float v = theta / g_PI;

    return float2(u, v);
}

[numthreads(8, 8, 1)]
void CS_Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float4> inEquirectangularTexture = ResourceDescriptorHeap[GetRootConstant(joint::EquirectangularToCubePassRC_EquirectangularTexture)];
    RWTexture2DArray<float4> outCubeTexture = ResourceDescriptorHeap[GetRootConstant(joint::EquirectangularToCubePassRC_OutCubeTexture)];

    float width;
    float height;
    float depth;
    outCubeTexture.GetDimensions(width, height, depth);

    const float2 faceUV = (dispatchThreadID.xy + 0.5f) / float2(width, height);
    const float3 cubeUV = GetCubeUV(faceUV, dispatchThreadID.z);

    const float3 direction = normalize(cubeUV);
    const float2 spherical = ConvertUnitCartesianToSpherical(direction);
    const float2 equirectangularUV = ConvertSphericalToUV(spherical.x, spherical.y);

    const float4 color = inEquirectangularTexture.SampleLevel(g_Anisotropic16WrapSampler, equirectangularUV, 0.0f);

    outCubeTexture[dispatchThreadID] = color;
}