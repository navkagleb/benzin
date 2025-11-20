#include "joint/geometry_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "gbuffer.hlsli"
#include "joint/mesh_types.hpp"

#define ALPHA_TEST_ENABLED defined(ALPHA_TEST)
#define MESH_PIPELINE_ENABLED defined(MESH_PIPELINE)
#define COMPUTE_CULLING_ENABLED defined(COMPUTE_CULLING)
#define LATE_CULLING_ENABLED defined(LATE_CULLING)

BenzinDeclareRootResource(StructuredBuffer<joint::Material>, g_Materials, joint::GeometryResources::Materials);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshDraw>, g_Draws, joint::GeometryResources::MeshDraws);

bool IsFrustumCulled(joint::MeshDraw draw, float3 center, float radius)
{
    bool isVisible = true;

    float4 viewCenter = mul(float4(center, 1.0), draw.m_LocalToWorld);
    viewCenter = mul(viewCenter, GetCameraConsts().m_WorldToView);

    const float worldRadius = radius * draw.m_LocalToWorldScale;

    [unroll]
    for (uint i = 0; i < 6; ++i)
    {
        const float4 viewPlane = GetCameraConsts().m_ViewFrustumPlanes[i];
        const float distanceToPlane = dot(viewPlane.xyz, viewCenter.xyz) + viewPlane.w;

        isVisible &= distanceToPlane < worldRadius;
    }

    return !isVisible;
}

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

VsOutput ProcessVertex(joint::MeshVertex vertex, uint drawIndex)
{
    const joint::MeshDraw draw = g_Draws[drawIndex];

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

BenzinDeclareRootResource(StructuredBuffer<joint::MeshDispatch>, g_Dispatches, joint::GeometryResources::MeshDispathes);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshVertex>, g_Vertices, joint::GeometryResources::Vertices);
BenzinDeclareRootResource(StructuredBuffer<joint::Meshlet>, g_Meshlets, joint::GeometryResources::Meshlets);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshletCullVolume>, g_MeshletCullVolumes, joint::GeometryResources::MeshletCullVolumes);
BenzinDeclareRootResource(Buffer<uint>, g_MeshletVertexIndices, joint::GeometryResources::MeshletVertexIndices);
BenzinDeclareRootResource(Buffer<uint>, g_MeshletIndices, joint::GeometryResources::MeshletIndices); // uint8_t

#define MESH_STATS_ENABLED 1
#define g_AmplificationGroupSize 32

struct MeshPayload
{
    uint m_MeshDispatchIndices[g_AmplificationGroupSize];
};

groupshared MeshPayload g_MeshPayload;

[NumThreads(g_AmplificationGroupSize, 1, 1)]
void AsMain(uint dispatchIndex : SV_DispatchThreadID)
{
    bool isVisible = dispatchIndex < BenzinGetRootConstant(joint::GeometryResources::MeshDispatchCount);

    const joint::MeshDispatch dispatch = g_Dispatches[dispatchIndex];

#if MESH_STATS_ENABLED
    const joint::Meshlet meshlet = g_Meshlets[dispatch.m_MeshletIndex];
    InterlockedAddToStat(joint::ReadbackStat::Geometry_TotalMeshletCount, isVisible);
    InterlockedAddToStat(joint::ReadbackStat::Geometry_TotalTriangleCount, isVisible * meshlet.m_TriangleCount);
#endif

    if (g_FrameConsts.m_IsFrustumCullingEnabled && isVisible)
    {
        const joint::MeshDraw draw = g_Draws[dispatch.m_MeshDrawIndex];
        const joint::MeshletCullVolume cullVolume = g_MeshletCullVolumes[dispatch.m_MeshletIndex];
    
        isVisible = !IsFrustumCulled(draw, cullVolume.m_Center, cullVolume.m_Radius);
    }

#if MESH_STATS_ENABLED
    InterlockedAddToStat(joint::ReadbackStat::Geometry_RenderedMeshletCount, isVisible);
    InterlockedAddToStat(joint::ReadbackStat::Geometry_RenderedTriangleCount, isVisible * meshlet.m_TriangleCount);
#endif

    if (isVisible)
    {
        const uint index = WavePrefixCountBits(isVisible);
        g_MeshPayload.m_MeshDispatchIndices[index] = dispatchIndex;
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
    const uint dispatchIndex = payload.m_MeshDispatchIndices[gid];
    const joint::MeshDispatch dispatch = g_Dispatches[dispatchIndex];
    const joint::Meshlet meshlet = g_Meshlets[dispatch.m_MeshletIndex];

    SetMeshOutputCounts(meshlet.m_VertexCount, meshlet.m_TriangleCount);

    if (gtid < meshlet.m_VertexCount)
    {
        const uint vertexIndex = g_MeshletVertexIndices[meshlet.m_VertexOffset + gtid];
        const joint::MeshVertex vertex = g_Vertices[vertexIndex];

        vertices[gtid] = ProcessVertex(vertex, dispatch.m_MeshDrawIndex);
    }

    if (gtid < meshlet.m_TriangleCount)
    {
        const uint triangleIndex = meshlet.m_IndexOffset + gtid * 3;
        const uint3 indices = uint3(g_MeshletIndices[triangleIndex + 0], g_MeshletIndices[triangleIndex + 1], g_MeshletIndices[triangleIndex + 2]);

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

VsOutput VsMain(VsInput vertex, uint vertexIndex : SV_VertexID)
{
    const uint drawIndex = BenzinGetRootConstant(joint::GeometryResources::MeshDrawIndex);
    return ProcessVertex((joint::MeshVertex)vertex, drawIndex);
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
    const joint::Material material = g_Materials[input.m_MaterialIndex];

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

#if COMPUTE_CULLING_ENABLED

BenzinDeclareRootResource(StructuredBuffer<joint::MeshDraw>, g_MeshDraws, joint::ComputeCullingResources::MeshDraws);
BenzinDeclareRootResource(StructuredBuffer<joint::Mesh>, g_Meshes, joint::ComputeCullingResources::Meshes);
#if LATE_CULLING_ENABLED
BenzinDeclareRootResource(RWBuffer<uint>, g_VisibilityBuffer, joint::ComputeCullingResources::VisibilityBuffer);
#else
BenzinDeclareRootResource(Buffer<uint>, g_VisibilityBuffer, joint::ComputeCullingResources::VisibilityBuffer);
#endif
BenzinDeclareRootResource(RWStructuredBuffer<joint::MeshDrawCmd>, g_DrawCmds, joint::ComputeCullingResources::MeshDrawCmds);
BenzinDeclareRootResource(RWBuffer<uint>, g_CmdCounter, joint::ComputeCullingResources::MeshDrawCmdCounter);

[numthreads(64, 1, 1)]
void CsMain(uint dtid : SV_DispatchThreadID)
{
    if (dtid >= BenzinGetRootConstant(joint::ComputeCullingResources::MeshDrawCount))
        return;

    const joint::MeshDraw draw = g_MeshDraws[dtid];
    const joint::Mesh mesh = g_Meshes[draw.m_MeshIndex];

#if LATE_CULLING_ENABLED
    bool isVisible = true;
#else
    bool isVisible = g_VisibilityBuffer[dtid];
#endif

    if (isVisible)
    {
        isVisible &= !IsFrustumCulled(draw, mesh.m_Center, mesh.m_Radius);
    }

#if LATE_CULLING_ENABLED
    const bool isDrawNeeded = isVisible && !g_VisibilityBuffer[dtid];
#else
    const bool isDrawNeeded = isVisible;
#endif

    if (isDrawNeeded)
    {
        joint::MeshDrawCmd cmd = (joint::MeshDrawCmd)0;
        cmd.m_DrawIndex = dtid;
        cmd.m_IndexCountPerInstance = mesh.m_IndexCount;
        cmd.m_InstanceCount = 1;
        cmd.m_StartIndexLocation = mesh.m_IndexOffset;
        cmd.m_BaseVertexLocation = mesh.m_VertexOffset;
        cmd.m_StartInstanceLocation = 0;

        uint cmdIndex;
        InterlockedAdd(g_CmdCounter[0], 1, cmdIndex);

        g_DrawCmds[cmdIndex] = cmd;
    }

#if LATE_CULLING_ENABLED
    g_VisibilityBuffer[dtid] = isVisible;
#endif
}

#endif // COMPUTE_CULLING_ENABLED
