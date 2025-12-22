#include "joint/geometry_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "gbuffer.hlsli"
#include "joint/mesh_types.hpp"

#define ALPHA_TEST_ENABLED defined(ALPHA_TEST)
#define MESH_PIPELINE_ENABLED defined(MESH_PIPELINE)


struct VsOutput
{
    float4 m_ClipPosition : SV_Position;
    float3 m_WorldPosition : sem_WorldPosition;
    float m_ViewDepth : sem_ViewDepth;
    float3 m_PrevViewPosition : sem_PrevViewPosition;
    float3 m_WorldNormal : sem_WorldNormal;
    float2 m_Uv : sem_Uv;
    nointerpolation uint m_MaterialIndex : sem_MaterialIndex;
};

VsOutput ProcessVertex(joint::MeshVertex vertex)
{
    StructuredBuffer<joint::MeshDraw> draws = BenzinGetRootResource(joint::GeometryRootParam::MeshDraws);
    const uint drawIndex = BenzinGetRootConstant(joint::GeometryRootParam::MeshDrawIndex);
    const joint::MeshDraw draw = draws[drawIndex];

    const float4 worldPosition = mul(float4(vertex.m_Position, 1.0), draw.m_LocalToWorld);
    const float4 prevWorldPosition = mul(float4(vertex.m_Position, 1.0), draw.m_PrevLocalToWorld);

    VsOutput output = (VsOutput)0;
    output.m_ClipPosition = mul(worldPosition, GetCameraConsts().m_WorldToClip);
    output.m_WorldPosition = worldPosition.xyz;
    output.m_ViewDepth = mul(worldPosition, GetCameraConsts().m_WorldToView).z;
    output.m_PrevViewPosition = mul(prevWorldPosition, GetPrevCameraConsts().m_WorldToView).xyz;
    output.m_WorldNormal = normalize(mul(vertex.m_Normal, (float3x3)draw.m_LocalToWorld)); // NOTE: Assumes uniform scale
    output.m_Uv = vertex.m_Uv;
    output.m_MaterialIndex = draw.m_MaterialIndex;

    return output;
}

#define g_AmplificationGroupSize 32

#if MESH_PIPELINE_ENABLED

// Sources:
// - Two-Pass Hierarchical Z-Buffer Occlusion Culling: https://medium.com/@Lucmomber/two-pass-hierarchical-z-buffer-occlusion-culling-93171c5a9808
// - Depth Precision Visualized (Reversed-Z): https://developer.nvidia.com/content/depth-precision-visualized
// - GDC 2024 - Mesh Shaders in AMD RDNA™ 3 Architecture: https://www.youtube.com/watch?v=MQv76-q2cm8
// - milkru/vulkanizer: https://github.com/milkru/vulkanizer/blob/main/src/shaders/generate_draws.comp
// - TODO - Using Mesh Shaders for Professional Graphics: https://developer.nvidia.com/blog/using-mesh-shaders-for-professional-graphics/
// - TODO - NVIDIA Sharing New Details about Mesh Shading at SIGGRAPH 2019: https://developer.nvidia.com/blog/siggraph-2019-mesh-shading-talk/
// - TODO - Direct3D 12: Long Way to Access Data: https://asawicki.info/news_1754_direct3d_12_long_way_to_access_data
// - TODO - Efficient Use of GPU Memory in Modern Games - Digital Dragons 2021: https://gpuopen.com/videos/efficient-use-of-gpu-memory-digital-dragons/
// - TODO - D3D12 Memory Allocator: https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator



struct MeshPayload
{
    uint m_MeshletIndices[g_AmplificationGroupSize];
};

groupshared MeshPayload g_MeshPayload;

[NumThreads(g_AmplificationGroupSize, 1, 1)]
void AsMain(uint dtid : SV_DispatchThreadID)
{
    const uint meshletOffset = BenzinGetRootConstant(joint::GeometryRootParam::MeshletOffset);
    const uint meshletCount = BenzinGetRootConstant(joint::GeometryRootParam::MeshletCount);

    bool isVisible = dtid < meshletCount;

    if (isVisible)
    {
        const uint index = WavePrefixCountBits(isVisible);
        g_MeshPayload.m_MeshletIndices[index] = dtid + meshletOffset;
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
    out indices uint3 triangles[(uint)joint::MeshletConsts::MaxTriangleCount])
{
    StructuredBuffer<joint::Meshlet> meshlets = BenzinGetRootResource(joint::GeometryRootParam::Meshlets);

#if 0
    const uint meshletIndex = payload.m_MeshletIndices[gid];
    const joint::Meshlet meshlet = meshlets[meshletIndex];
#else
    const uint meshletOffset = BenzinGetRootConstant(joint::GeometryRootParam::MeshletOffset);
    const uint meshletCount = BenzinGetRootConstant(joint::GeometryRootParam::MeshletCount);

    if (gid >= meshletCount)
        return;

    const joint::Meshlet meshlet = meshlets[gid + meshletOffset];
#endif

    SetMeshOutputCounts(meshlet.m_VertexCount, meshlet.m_TriangleCount);

    StructuredBuffer<joint::MeshVertex> meshVertices = BenzinGetRootResource(joint::GeometryRootParam::Vertices);
    Buffer<uint> meshletVertexIndices = BenzinGetRootResource(joint::GeometryRootParam::MeshletVertexIndices);
    Buffer<uint> meshletIndices = BenzinGetRootResource(joint::GeometryRootParam::MeshletIndices); // uint8_t

    if (gtid < meshlet.m_VertexCount)
    {
        const uint vertexIndex = meshletVertexIndices[meshlet.m_VertexOffset + gtid];
        const joint::MeshVertex vertex = meshVertices[vertexIndex];

        vertices[gtid] = ProcessVertex(vertex);
    }

    if (gtid < meshlet.m_TriangleCount)
    {
        const uint triangleIndex = meshlet.m_IndexOffset + gtid * 3;
        const uint3 indices = uint3(meshletIndices[triangleIndex + 0], meshletIndices[triangleIndex + 1], meshletIndices[triangleIndex + 2]);

        triangles[gtid] = indices;
    }
}
#endif // MESH_PIPELINE_ENABLED

// Must match with joint::MeshVertex
struct VsInput
{
    float3 m_Position : sem_Position;
    float3 m_Normal : sem_Normal;
    float2 m_Uv : sem_Uv;
};

VsOutput VsMain(VsInput vertex)
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
    StructuredBuffer<joint::Material> materials = BenzinGetRootResource(joint::GeometryRootParam::Materials);
    const joint::Material material = materials[input.m_MaterialIndex];

    float3 albedo = material.m_AlbedoFactor.rgb;
    if (material.m_AlbedoTextureHeapIndex != g_MaxU32)
    {
        Texture2D<float4> albedoTexture = ResourceDescriptorHeap[NonUniformResourceIndex(material.m_AlbedoTextureHeapIndex)];
        const float4 albedoSample = albedoTexture.Sample(g_LinearWrapSampler, input.m_Uv);

#if ALPHA_TEST_ENABLED
        if (albedoSample.a < material.m_AlphaCutoff)
            discard;
#endif

        albedo *= albedoSample.rgb;
    }

    GBuffer gbuffer = (GBuffer)0;
    gbuffer.m_Albedo = albedo;
    gbuffer.m_ViewDepth = input.m_ViewDepth;
    gbuffer.m_WorldNormal = normalize(input.m_WorldNormal);
    gbuffer.m_Roughness = material.m_RoughnessFactor;
    gbuffer.m_Emissive = material.m_EmissiveFactor;
    gbuffer.m_Metallic = material.m_MetalnessFactor;

    gbuffer.m_Albedo = gbuffer.m_WorldNormal * 0.5 + 0.5;

    if (material.m_NormalTextureHeapIndex != g_MaxU32)
    {
        Texture2D<float4> normalTexture = ResourceDescriptorHeap[NonUniformResourceIndex(material.m_NormalTextureHeapIndex)];

        float3 tangentSpaceNormal = normalTexture.Sample(g_LinearWrapSampler, input.m_Uv).xyz;
        tangentSpaceNormal = 2.0 * tangentSpaceNormal - 1.0;
        tangentSpaceNormal.xy *= material.m_NormalScale;
        tangentSpaceNormal = normalize(tangentSpaceNormal);

        const float3 worldViewVector = normalize(GetCameraConsts().m_WorldPosition - input.m_WorldPosition);
        const float3x3 tbn = CotangentFrame(gbuffer.m_WorldNormal, -worldViewVector, input.m_Uv);

        gbuffer.m_WorldNormal = normalize(mul(tangentSpaceNormal, tbn));
    }

    if (material.m_EmissiveTextureHeapIndex != g_MaxU32)
    {
        Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[NonUniformResourceIndex(material.m_EmissiveTextureHeapIndex)];
        const float3 emissiveSample = emissiveTexture.Sample(g_LinearWrapSampler, input.m_Uv).rgb;

        gbuffer.m_Emissive *= emissiveSample;
    }

    if (material.m_MetallicRoughnessTextureHeapIndex != g_MaxU32)
    {
        Texture2D<float4> metallicRoughnessTexture = ResourceDescriptorHeap[NonUniformResourceIndex(material.m_MetallicRoughnessTextureHeapIndex)];
        const float metallicSample = metallicRoughnessTexture.Sample(g_LinearWrapSampler, input.m_Uv).b;
        const float roughnessSample = metallicRoughnessTexture.Sample(g_LinearWrapSampler, input.m_Uv).g;

        gbuffer.m_Metallic *= metallicSample;
        gbuffer.m_Roughness *= roughnessSample;
    }

    CalcGBufferMv(input.m_ClipPosition.xy, input.m_ViewDepth, input.m_PrevViewPosition, gbuffer);

    return PackGBuffer(gbuffer);
}
