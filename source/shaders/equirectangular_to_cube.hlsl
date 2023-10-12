#include "common.hlsli"
#include "cube_helper.hlsli"

namespace internal
{

    float2 ConvertUnitCartesianToSpherical(float3 cartesian)
    {
        const float phi = atan2(cartesian.z, cartesian.x); // Azimuthal angle
        const float theta = acos(cartesian.y); // Polar angle

        return float2(phi, theta);
    }

    float2 ConvertSphericalToUV(float phi, float theta)
    {
        const float u = 0.5f - phi / common::g_2PI;
        const float v = theta / common::g_PI;

        return float2(u, v);
    }

} // namespace internal

enum : uint32_t
{
    g_InEquirectangularTextureIndex,
    g_OutCubeTextureIndex,
};

[numthreads(8, 8, 1)]
void CS_Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float4> inEquirectangularTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_InEquirectangularTextureIndex)];
    RWTexture2DArray<float4> outCubeTexture = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_OutCubeTextureIndex)];

    float width;
    float height;
    float depth;
    outCubeTexture.GetDimensions(width, height, depth);

    const float2 faceUV = (dispatchThreadID.xy + 0.5f) / float2(width, height);
    const float3 cubeUV = cube_helper::GetCubeUV(faceUV, dispatchThreadID.z);

    const float3 direction = normalize(cubeUV);
    const float2 spherical = internal::ConvertUnitCartesianToSpherical(direction);
    const float2 equirectangularUV = internal::ConvertSphericalToUV(spherical.x, spherical.y);

    const float4 color = inEquirectangularTexture.SampleLevel(common::g_Anisotropic16WrapSampler, equirectangularUV, 0.0f);

    outCubeTexture[dispatchThreadID] = color;
}