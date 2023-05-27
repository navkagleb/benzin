#include "common.hlsli"
#include "gbuffer_utils.hlsli"

static const uint32_t g_InvalidTextureIndex = -1;

float3 ExpandNormal(float2 xyNormal)
{
    return float3(xyNormal.x, xyNormal.y, sqrt(1.0 - xyNormal.x * xyNormal.x - xyNormal.y * xyNormal.y));
}

float3x3 CotangentFrame(float3 N, float3 p, float2 uv)
{
    // http://www.thetenthplanet.de/archives/1180

    // Get edge vectors of the pixel triangle
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    // Solve the linear system
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // Construct a scale-invariant frame
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    return float3x3(T * invmax, B * invmax, N);
}

float3x3 GetTBNBasis(float3 position, float3 normal, float2 uv)
{
    float3 dp1 = ddx(position);
    float3 dp2 = ddy(position);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float det = duv1.x * duv2.y - duv2.x * duv1.y;
    float invdet = 1.0 / det;

    float3 tangent = normalize((dp1 * duv2.y - dp2 * duv1.y) * invdet);
    float3 bitangent = normalize((-dp1 * duv2.x + dp2 * duv1.x) * invdet);

    return float3x3(tangent, bitangent, normal);
}

enum : uint32_t
{
    g_PassBufferIndex,

    g_EntityBufferIndex,
    g_EntityIndex,

    g_VertexBufferIndex,
    g_IndexBufferIndex,
    g_MeshPrimitiveBufferIndex,

    g_DrawPrimitiveBufferIndex,
    g_DrawPrimitiveIndex,
    g_NodeBufferIndex,
    g_NodeIndex,
    g_MaterialBufferIndex,
};

struct MeshPrimitive
{
    uint32_t StartVertex;
    uint32_t StartIndex;
    uint32_t IndexCount;
};

struct DrawPrimitive
{
    uint32_t MeshPrimitiveIndex;
    uint32_t MaterialIndex;
};

struct MeshVertex
{
    float3 LocalPosition;
    float3 LocalNormal;
    float2 TexCoord;
};

struct PassData
{
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;
    float3 WorldCameraPosition;
};

struct NodeData
{
    float4x4 LocalMatrix;
    float4x4 InverseLocalMatrix;
};

struct EntityData
{
    float4x4 WorldMatrix;
    float4x4 InverseWorldMatrix;
};

struct Material
{
    uint32_t AlbedoTextureIndex;
    uint32_t NormalTextureIndex;
    uint32_t MetalRoughnessTextureIndex;
    uint32_t AOTextureIndex;
    uint32_t EmissiveTextureIndex;

    float4 AlbedoFactor;
    float AlphaCutoff;
    float NormalScale;
    float MetalnessFactor;
    float RoughnessFactor;
    float OcclusionStrength;
    float3 EmissiveFactor;
};

struct VS_Output
{
    float4 HomogeneousPosition : SV_Position;
    float3 WorldPosition : WorldPosition;
    float3 WorldNormal : WorldNormal;
    float2 TexCoord : TexCoord;
};

VS_Output VS_Main(uint32_t vertexID : SV_VertexID)
{
    ConstantBuffer<PassData> passData = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_PassBufferIndex)];

    StructuredBuffer<MeshVertex> vertexBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_VertexBufferIndex)];
    StructuredBuffer<uint32_t> indexBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_IndexBufferIndex)];

    StructuredBuffer<DrawPrimitive> drawPrimitiveBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_DrawPrimitiveBufferIndex)];
    StructuredBuffer<MeshPrimitive> meshPrimitiveBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_MeshPrimitiveBufferIndex)];

    StructuredBuffer<NodeData> nodeBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_NodeBufferIndex)];
    StructuredBuffer<EntityData> entityBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_EntityBufferIndex)];

    const uint32_t drawPrimitiveIndex = BENZIN_GET_ROOT_CONSTANT(g_DrawPrimitiveIndex);
    const uint32_t nodeIndex = BENZIN_GET_ROOT_CONSTANT(g_NodeIndex);
    const uint32_t entityIndex = BENZIN_GET_ROOT_CONSTANT(g_EntityIndex);

    const DrawPrimitive drawPrimitive = drawPrimitiveBuffer[drawPrimitiveIndex];
    const MeshPrimitive meshPrimitive = meshPrimitiveBuffer[drawPrimitive.MeshPrimitiveIndex];
    const uint32_t index = indexBuffer[meshPrimitive.StartIndex + vertexID];
    const MeshVertex vertex = vertexBuffer[meshPrimitive.StartVertex + index];

    const NodeData nodeData = nodeBuffer[nodeIndex];
    const EntityData entityData = entityBuffer[entityIndex];

    const float4 localPosition = mul(float4(vertex.LocalPosition, 1.0f), nodeData.LocalMatrix);
    const float4 worldPosition = mul(localPosition, entityData.WorldMatrix);

    const float4x4 normalLocalMatrix = transpose(nodeData.InverseLocalMatrix);
    const float4x4 normalWorldMatrix = transpose(entityData.InverseWorldMatrix);
    const float3 localNormal = mul(vertex.LocalNormal, (float3x3)normalLocalMatrix);
    const float3 worldNormal = mul(localNormal, (float3x3)normalWorldMatrix);

    VS_Output output = (VS_Output)0;
    output.HomogeneousPosition = mul(worldPosition, passData.ViewProjection);
    output.WorldPosition = worldPosition.xyz;
    output.WorldNormal = worldNormal;
    output.TexCoord = vertex.TexCoord;

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
    ConstantBuffer<PassData> passData = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_PassBufferIndex)];

    StructuredBuffer<DrawPrimitive> drawPrimitiveBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_DrawPrimitiveBufferIndex)];
    StructuredBuffer<MeshPrimitive> meshPrimitiveBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_MeshPrimitiveBufferIndex)];
    StructuredBuffer<Material> materialBuffer = ResourceDescriptorHeap[BENZIN_GET_ROOT_CONSTANT(g_MaterialBufferIndex)];

    const uint32_t drawPrimitiveIndex = BENZIN_GET_ROOT_CONSTANT(g_DrawPrimitiveIndex);

    const DrawPrimitive drawPrimitive = drawPrimitiveBuffer[drawPrimitiveIndex];
    const Material material = materialBuffer[drawPrimitive.MaterialIndex];

    gbuffer::Unpacked gbuffer;
    gbuffer.Albedo = material.AlbedoFactor;
    gbuffer.WorldNormal = normalize(input.WorldNormal);
    gbuffer.Emissive = material.EmissiveFactor;
    gbuffer.Roughness = material.RoughnessFactor;
    gbuffer.Metalness = material.MetalnessFactor;

    if (material.AlbedoTextureIndex != g_InvalidTextureIndex)
    {
        Texture2D<float4> albedoTexture = ResourceDescriptorHeap[material.AlbedoTextureIndex];
        const float4 albedoSample = albedoTexture.Sample(common::g_LinearWrapSampler, input.TexCoord);

        if (albedoSample.a < material.AlphaCutoff)
        {
            discard;
        }

        gbuffer.Albedo *= albedoSample;
    }

    if (material.NormalTextureIndex != g_InvalidTextureIndex)
    {
        Texture2D<float4> normalTexture = ResourceDescriptorHeap[material.NormalTextureIndex];

        float3 normalSample = normalTexture.Sample(common::g_LinearWrapSampler, input.TexCoord).xyz;
        normalSample = 2.0 * normalSample - 1.0;
        normalSample = normalize(normalSample * float3(material.NormalScale, material.NormalScale, 1.0));
        //normalSample = ExpandNormal(normalSample.xy);

        const float3 worldViewDirection = normalize(passData.WorldCameraPosition - input.WorldPosition);

        const float3x3 tbn = CotangentFrame(gbuffer.WorldNormal, -worldViewDirection, input.TexCoord);
        //const float3x3 tbn = GetTBNBasis(worldPosition, input.WorldNormal, input.TexCoord);

        gbuffer.WorldNormal = normalize(mul(normalSample, tbn));
    }

    if (material.EmissiveTextureIndex != g_InvalidTextureIndex)
    {
        Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[material.EmissiveTextureIndex];
        const float3 emissiveSample = emissiveTexture.Sample(common::g_LinearWrapSampler, input.TexCoord).rgb;

        gbuffer.Emissive *= emissiveSample;
    }

    if (material.MetalRoughnessTextureIndex != g_InvalidTextureIndex)
    {
        Texture2D<float4> metalRoughnessTexture = ResourceDescriptorHeap[material.MetalRoughnessTextureIndex];
        const float roughnessSample = metalRoughnessTexture.Sample(common::g_LinearWrapSampler, input.TexCoord).g;
        const float metalnessSample = metalRoughnessTexture.Sample(common::g_LinearWrapSampler, input.TexCoord).b;

        gbuffer.Roughness *= roughnessSample;
        gbuffer.Metalness *= metalnessSample;
    }

    gbuffer::Packed packed = gbuffer::Pack(gbuffer);

    PS_Output output = (PS_Output)0;
    output.Color0 = packed.Color0;
    output.Color1 = packed.Color1;
    output.Color2 = packed.Color2;
    output.Color3 = packed.Color3;

    return output;
}
