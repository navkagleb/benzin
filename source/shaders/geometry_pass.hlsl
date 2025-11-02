#include "joint/geometry_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "culling.hlsli"
#include "gbuffer.hlsli"
#include "gpu_print.hlsli"
#include "joint/mesh_types.hpp"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(StructuredBuffer<joint::Material>, g_Materials, joint::GeometryResources::Materials);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshDraw>, g_MeshDraws, joint::GeometryResources::MeshDraws);

#if defined(MESH_PIPELINE)
BenzinDeclareRootResource(StructuredBuffer<joint::MeshVertex>, g_Vertices, joint::GeometryResources::Vertices);
BenzinDeclareRootResource(StructuredBuffer<joint::Meshlet>, g_Meshlets, joint::GeometryResources::Meshlets);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshletCullVolume>, g_MeshletCullVolumes, joint::GeometryResources::MeshletCullVolumes);
BenzinDeclareRootResource(Buffer<uint>, g_MeshletVertexIndices, joint::GeometryResources::MeshletVertexIndices);
BenzinDeclareRootResource(Buffer<uint>, g_MeshletIndices, joint::GeometryResources::MeshletIndices); // uint8_t
#endif

struct VsOutput
{
    float4 m_ClipPosition : SV_Position;
    float3 m_WorldPosition : WorldPosition;
    float m_ViewDepth : ViewDepth;
    float3 m_PrevViewPosition : PrevViewPosition;
    float3 m_WorldNormal : WorldNormal;
    float2 m_Uv : Uv;
};

VsOutput ProcessVertex(joint::MeshVertex vertex)
{
    const uint drawIndex = BenzinGetRootConstant(joint::GeometryResources::MeshDrawIndex);
    const joint::MeshDraw draw = g_MeshDraws[drawIndex];

    const float4 worldPosition = mul(float4(vertex.m_Position, 1.0), draw.m_LocalToWorld);
    const float4 prevWorldPosition = mul(float4(vertex.m_Position, 1.0), draw.m_PrevLocalToWorld);

    VsOutput output = (VsOutput)0;
    output.m_ClipPosition = mul(worldPosition, GetCameraConsts().WorldToClip);
    output.m_WorldPosition = worldPosition.xyz;
    output.m_ViewDepth = mul(worldPosition, GetCameraConsts().WorldToView).z;
    output.m_PrevViewPosition = mul(prevWorldPosition, GetPrevCameraConsts().WorldToView).xyz;
    output.m_WorldNormal = normalize(mul(vertex.m_Normal, (float3x3)draw.m_LocalToWorld)); // NOTE: Assumes uniform scale
    output.m_Uv = vertex.m_Uv;

    return output;
}

#if defined(MESH_PIPELINE)
struct MeshPayload
{
    uint m_MeshletIndices[(uint)joint::MeshletConsts::AsGroupSize];
};

groupshared MeshPayload g_MeshPayload;

[NumThreads((uint)joint::MeshletConsts::AsGroupSize, 1, 1)]
void AsMain(uint localMeshletIndex : SV_DispatchThreadID)
{
    bool isVisible = localMeshletIndex < BenzinGetRootConstant(joint::GeometryResources::PartMeshletCount);
    const uint meshletIndex = localMeshletIndex + BenzinGetRootConstant(joint::GeometryResources::PartMeshletOffset);

    if (isVisible)
    {
        const uint index = WavePrefixCountBits(isVisible);
        g_MeshPayload.m_MeshletIndices[index] = meshletIndex;
    }

    const uint visibleCount = WaveActiveCountBits(isVisible);
    DispatchMesh(visibleCount, 1, 1, g_MeshPayload);
}

[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void MsMain(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload MeshPayload payload,
    out vertices VsOutput vertices[(uint)joint::MeshletConsts::MaxVertexCount],
    out indices uint3 triangles[(uint)joint::MeshletConsts::MaxTriangleCount]
)
{
    const uint meshletIndex = payload.m_MeshletIndices[gid];
    const joint::Meshlet meshlet = g_Meshlets[meshletIndex];

    SetMeshOutputCounts(meshlet.m_VertexCount, meshlet.m_TriangleCount);

    if (gtid < meshlet.m_VertexCount)
    {
        const uint vertexIndex = g_MeshletVertexIndices[meshlet.m_VertexOffset + gtid];
        const joint::MeshVertex vertex = g_Vertices[vertexIndex];

        vertices[gtid] = ProcessVertex(vertex);
    }

    if (gtid < meshlet.m_TriangleCount)
    {
        const uint triangleIndex = meshlet.m_IndexOffset + gtid * 3;
        const uint3 indices = uint3(g_MeshletIndices[triangleIndex + 0], g_MeshletIndices[triangleIndex + 1], g_MeshletIndices[triangleIndex + 2]);

        triangles[gtid] = indices;
    }
}
#endif // defined(MESH_PIPELINE)

// Must match with joint::MeshVertex
struct VsInput
{
    float3 m_Position : Position;
    float3 m_Normal : Normal;
    float2 m_Uv : Uv;
};

VsOutput VsMain(VsInput vertex, uint vertexIndex : SV_VertexID)
{
    return ProcessVertex((joint::MeshVertex)vertex);
}

float3x3 CotangentFrame(float3 worldNormal, float3 p, float2 uv)
{
    // Ref: http://www.thetenthplanet.de/archives/1180

    // Get edge vectors of the pixel triangle
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    // Solve the linear system
    float3 dp2perp = cross(dp2, worldNormal);
    float3 dp1perp = cross(worldNormal, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // Construct a scale-invariant frame
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    return float3x3(T * invmax, B * invmax, worldNormal);
}

PackedGBuffer PsMain(VsOutput input)
{
    const uint drawIndex = BenzinGetRootConstant(joint::GeometryResources::MeshDrawIndex);
    const joint::MeshDraw draw = g_MeshDraws[drawIndex];
    const joint::Material material = g_Materials[draw.m_MaterialIndex];

    float3 albedo = material.m_AlbedoFactor.rgb;
    if (material.m_AlbedoTextureHeapIndex != g_MaxU32)
    {
        Texture2D<float4> albedoTexture = ResourceDescriptorHeap[material.m_AlbedoTextureHeapIndex];
        const float4 albedoSample = albedoTexture.Sample(g_LinearWrapSampler, input.m_Uv);

#if defined(ALPHA_TEST)
        if (albedoSample.a < material.m_AlphaCutoff)
            discard;
#endif

        albedo *= albedoSample.rgb;
    }

    GBuffer gbuffer = (GBuffer)0;
    gbuffer.Albedo = albedo;
    gbuffer.ViewDepth = input.m_ViewDepth;
    gbuffer.WorldNormal = normalize(input.m_WorldNormal);
    gbuffer.Roughness = material.m_RoughnessFactor;
    gbuffer.Emissive = material.m_EmissiveFactor;
    gbuffer.Metallic = material.m_MetalnessFactor;

    if (material.m_NormalTextureHeapIndex != g_MaxU32)
    {
        Texture2D<float4> normalTexture = ResourceDescriptorHeap[material.m_NormalTextureHeapIndex];

        float3 tangentSpaceNormal = normalTexture.Sample(g_LinearWrapSampler, input.m_Uv).xyz;
        tangentSpaceNormal = 2.0 * tangentSpaceNormal - 1.0;
        tangentSpaceNormal.xy *= material.m_NormalScale;
        tangentSpaceNormal = normalize(tangentSpaceNormal);

        const float3 worldViewVector = normalize(GetCameraConsts().WorldPosition - input.m_WorldPosition);
        const float3x3 tbn = CotangentFrame(gbuffer.WorldNormal, -worldViewVector, input.m_Uv);

        gbuffer.WorldNormal = normalize(mul(tangentSpaceNormal, tbn));
    }

    if (material.m_EmissiveTextureHeapIndex != g_MaxU32)
    {
        Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[material.m_EmissiveTextureHeapIndex];
        const float3 emissiveSample = emissiveTexture.Sample(g_LinearWrapSampler, input.m_Uv).rgb;

        gbuffer.Emissive *= emissiveSample;
    }

    if (material.m_MetallicRoughnessTextureHeapIndex != g_MaxU32)
    {
        Texture2D<float4> metallicRoughnessTexture = ResourceDescriptorHeap[material.m_MetallicRoughnessTextureHeapIndex];
        const float metallicSample = metallicRoughnessTexture.Sample(g_LinearWrapSampler, input.m_Uv).b;
        const float roughnessSample = metallicRoughnessTexture.Sample(g_LinearWrapSampler, input.m_Uv).g;

        gbuffer.Metallic *= metallicSample;
        gbuffer.Roughness *= roughnessSample;
    }

    CalcGBufferMv(input.m_ClipPosition.xy, input.m_ViewDepth, input.m_PrevViewPosition, gbuffer);

    return PackGBuffer(gbuffer);
}
