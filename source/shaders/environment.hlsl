#include "common.hlsli"
#include "fullscreen_helper.hlsli"

enum : uint32_t
{
    g_PassBufferIndex,
    g_CubeTextureIndex,
};

struct PassData
{
    float4x4 InverseViewDirectionProjectionMatrix;
};

float4 PS_Main(fullscreen_helper::VS_Output input) : SV_Target
{
    ConstantBuffer<PassData> passData = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_PassBufferIndex)];
    TextureCube<float4> cubeMap = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_CubeTextureIndex)];

    const float4 worldPosition = mul(input.ClipPosition, passData.InverseViewDirectionProjectionMatrix);
    const float3 direction = normalize(worldPosition.xyz);

    const float4 color = cubeMap.Sample(common::g_LinearWrapSampler, direction);
    return color / (color + 1.0f);
}
