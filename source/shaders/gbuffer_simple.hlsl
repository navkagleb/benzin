#include "common.hlsli"
#include "gbuffer_utils.hlsli"

enum : uint32_t
{
    g_PassBufferIndex,

    g_EntityBufferIndex,
    g_EntityIndex,

    g_VertexBufferIndex,
    g_IndexBufferIndex,
};

struct MeshVertex
{
    float3 Position;
    float3 Normal;
    float2 UV;
};

struct PassData
{
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;
    float3 WorldCameraPosition;
};

struct EntityData
{
    float4x4 WorldMatrix;
    float4x4 InverseWorldMatrix;
};

struct VS_Output
{
    float4 HomogeneousPosition : SV_Position;
    float3 WorldPosition : WorldPosition;
    float3 WorldNormal : WorldNormal;
    float2 UV : UV;
};

VS_Output VS_Main(uint32_t vertexID : SV_VertexID)
{
    ConstantBuffer<PassData> passData = ResourceDescriptorHeap[BenzinGetRootConstant(g_PassBufferIndex)];

    StructuredBuffer<MeshVertex> vertexBuffer = ResourceDescriptorHeap[BenzinGetRootConstant(g_VertexBufferIndex)];
    StructuredBuffer<uint32_t> indexBuffer = ResourceDescriptorHeap[BenzinGetRootConstant(g_IndexBufferIndex)];
    StructuredBuffer<EntityData> entityBuffer = ResourceDescriptorHeap[BenzinGetRootConstant(g_EntityBufferIndex)];

    const uint32_t index = indexBuffer[vertexID];
    const MeshVertex vertex = vertexBuffer[index];
    const EntityData entityData = entityBuffer[BenzinGetRootConstant(g_EntityIndex)];

    const float4 worldPosition = mul(float4(vertex.Position, 1.0f), entityData.WorldMatrix);

    VS_Output output;
    output.HomogeneousPosition = mul(worldPosition, passData.ViewProjection);
    output.WorldPosition = worldPosition.xyz;
    output.WorldNormal = mul(vertex.Normal, (float3x3)transpose(entityData.InverseWorldMatrix));
    output.UV = vertex.UV;

    return output;
}

struct PS_Output
{
    float4 Color0 : SV_Target0;
    float4 Color1 : SV_Target1;
    float4 Color2 : SV_Target2;
    float4 Color3 : SV_Target3;
};

PS_Output PS_Main(VS_Output input)
{
    ConstantBuffer<PassData> passData = ResourceDescriptorHeap[BenzinGetRootConstant(g_PassBufferIndex)];

    gbuffer::Unpacked gbuffer;
    gbuffer.Albedo = float4(0.8f, 0.9f, 0.7f, 1.0f);
    gbuffer.WorldNormal = normalize(input.WorldNormal);
    gbuffer.Emissive = float3(0.0f, 0.0f, 0.0f);
    gbuffer.Roughness = 0.1f;
    gbuffer.Metalness = 0.7f;

    gbuffer::Packed packed = gbuffer::Pack(gbuffer);

    PS_Output output = (PS_Output)0;
    output.Color0 = packed.Color0;
    output.Color1 = packed.Color1;
    output.Color2 = packed.Color2;
    output.Color3 = packed.Color3;

    return output;
}
