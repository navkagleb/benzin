#include "common.hlsli"

uint GetPassBufferIndex() { return g_RootConstants.Constant0; }
uint GetPassBufferOffset() { return g_RootConstants.Constant1; }

uint GetEntityBufferIndex() { return g_RootConstants.Constant2; }
uint GetEntityBufferOffset() { return g_RootConstants.Constant3; }

uint GetEnvironmentMapIndex() { return g_RootConstants.Constant5; }

struct VS_INPUT
{
    float3 LocalPosition : Position;
    float LocalNormal : Normal;
    float2 TexCoord : TexCoord;
};

struct VS_OUTPUT
{
    float4 HomogeneousPosition : SV_Position;
    float3 LocalPosition : Position;
};

VS_OUTPUT VS_Main(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    StructuredBuffer<PassData> passBuffer = ResourceDescriptorHeap[GetPassBufferIndex()];
    StructuredBuffer<EntityData> entityBuffer = ResourceDescriptorHeap[GetEntityBufferIndex()];

    const PassData passData = passBuffer[GetPassBufferOffset()];
    const EntityData entityData = entityBuffer[GetEntityBufferOffset() + instanceID];

    VS_OUTPUT output = (VS_OUTPUT)0;
    output.LocalPosition = input.LocalPosition;

    float4 worldPosition = mul(float4(input.LocalPosition, 1.0f), entityData.World);
    worldPosition.xyz += passData.WorldCameraPosition;

    output.HomogeneousPosition = mul(worldPosition, passData.ViewProjection).xyww;

    return output;
}

float4 PS_Main(VS_OUTPUT input) : SV_Target
{
    TextureCube environmentMap = ResourceDescriptorHeap[GetEnvironmentMapIndex()];

    return environmentMap.Sample(g_LinearWrapSampler, input.LocalPosition);
}
