#include "joint/geometry_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "culling.hlsli"
#include "gbuffer.hlsli"
#include "gpu_print.hlsli"
#include "joint/mesh_types.hpp"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(StructuredBuffer<joint::Material>, g_Materials, joint::GeometryResources::Materials);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshDrawPart>, g_MeshDrawParts, joint::GeometryResources::MeshDrawParts);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshDraw>, g_MeshDraws, joint::GeometryResources::MeshDraws);

#if 0
BenzinDeclareRootResource(StructuredBuffer<joint::MeshVertex>, g_Vertices, joint::GeometryResources::Vertices);
BenzinDeclareRootResource(StructuredBuffer<joint::Meshlet>, g_Meshlets, joint::GeometryResources::Meshlets);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshletCullVolume>, g_MeshletCullVolumes, joint::GeometryResources::MeshletCullVolumes);
BenzinDeclareRootResource(Buffer<uint>, g_MeshletIndirectVertices, joint::GeometryResources::MeshletIndirectVertices);
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
    const uint drawPartIndex = BenzinGetRootConstant(joint::GeometryResources::MeshDrawPartIndex);

    const joint::MeshDraw draw = g_MeshDraws[drawIndex];
    const joint::MeshDrawPart drawPart = g_MeshDrawParts[drawPartIndex];

    const float4x4 localToWorld = mul(drawPart.m_ObjectToLocal, draw.m_LocalToWorld);
    const float4 worldPosition = mul(float4(vertex.Position, 1.0), localToWorld);
    const float4 prevWorldPosition = mul(float4(vertex.Position, 1.0), mul(drawPart.m_ObjectToLocal, draw.m_PrevLocalToWorld));

    VsOutput output = (VsOutput)0;
    output.m_ClipPosition = mul(worldPosition, GetCameraConsts().WorldToClip);
    output.m_WorldPosition = worldPosition.xyz;
    output.m_ViewDepth = mul(worldPosition, GetCameraConsts().WorldToView).z;
    output.m_PrevViewPosition = mul(prevWorldPosition, GetPrevCameraConsts().WorldToView).xyz;
    output.m_WorldNormal = normalize(mul(vertex.Normal, (float3x3)localToWorld)); // NOTE: Assumes uniform scale
    output.m_Uv = vertex.Uv;

    return output;
}

#if 0
int SignExtend8(uint x)
{
    return (int)(x << 24) >> 24; // shifts into sign bit, then back
}

void UnpackConeAxisAndCutoff(uint packed, out float3 coneAxis, out float coneCutoff)
{
    const int axis_x_s8 = SignExtend8((packed >> 0) & 0xFF);
    const int axis_y_s8 = SignExtend8((packed >> 8) & 0xFF);
    const int axis_z_s8 = SignExtend8((packed >> 16) & 0xFF); 
    const int cutoff_s8 = SignExtend8((packed >> 24) & 0xFF);

    coneAxis = float3(axis_x_s8, axis_y_s8, axis_z_s8) / 127.0;
    coneCutoff = float(cutoff_s8) / 127.0;
}

bool IsCulled(uint meshletIndex, uint instanceIndex)
{
    const float4x4 localToWorld = g_LocalToWorldMatrices[instanceIndex];
    const joint::MeshletCullVolume cullVolume = g_MeshletCullVolumes[meshletIndex];

    if (g_PassConsts0.IsBackfaceCullingEnabled)
    {
        float3 coneAxis;
        float coneCutoff;
        UnpackConeAxisAndCutoff(cullVolume.m_PackedAxisAndCutoff, coneAxis, coneCutoff);
        
        float3 coneApex = mul(float4(cullVolume.m_ConeApex, 1.0), localToWorld).xyz;
        coneAxis = normalize(mul(coneAxis, (float3x3)localToWorld));

        const float3 viewDir = normalize(GetCameraConsts().WorldPosition - coneApex);
        const float dotView = dot(viewDir, coneAxis);

        if (dotView < -coneCutoff)
            return true;
    }

    if (g_PassConsts0.IsFrustumCullingEnabled)
    {
        float4 center = mul(float4(cullVolume.m_Center, 1.0), localToWorld);
        center = mul(center, GetCameraConsts().WorldToView);

        const float radius = cullVolume.m_Radius * ExtractScale(localToWorld);

        bool isVisible = true;

        // The left/top/right/bottom plane culling utilizes frustum symmetry to cull against two planes at the same time
        isVisible = isVisible && center.z * GetCameraConsts().TanHalfFovX - abs(center.x) > -radius;
        isVisible = isVisible && center.z * GetCameraConsts().TanHalfFovY - abs(center.y) > -radius;

        // The near/far plane culling uses camera space Z directly
        // NOTE: because we use an infinite projection matrix, this may cull meshlets that belong to a mesh that straddles
        // the "far" plane. we could optionally remove the far check to be conservative
        isVisible = isVisible && center.z + radius > GetCameraConsts().NearPlane && center.z - radius < GetCameraConsts().FarPlane;

        if (!isVisible)
            return true;
    }

    return false;

#if 0
    const ScreenBounds bounds = CalcScreenBounds(worldBoundingSphere.xyz, worldBoundingSphere.w);
    if (g_PassConsts0.IsOcclusionCullingEnabled && IsOcclusionCulled(g_ReprojectedHzb, bounds))
        return true;

    return false;
#endif
}

struct MeshPayload
{
    uint m_MeshletIndices[(uint)joint::MeshletConsts::AsGroupSize];
    uint m_InstanceIndices[(uint)joint::MeshletConsts::AsGroupSize];
};

groupshared MeshPayload g_MeshPayload;

[NumThreads((uint)joint::MeshletConsts::AsGroupSize, 1, 1)]
void AsMain(uint dtid : SV_DispatchThreadID)
{
    InterlockedAddToStat(joint::ReadbackStat::Geometry_AsInvocationCount, 1);

    const uint meshletCountPerInstance = BenzinGetRootConstant(joint::GeometryResources::MeshletCountPerInstance);
    const uint totalMeshletCount = BenzinGetRootConstant(joint::GeometryResources::TotalMeshletCount);

    const uint instanceIndex = dtid / meshletCountPerInstance;
    const uint meshletIndex = dtid % meshletCountPerInstance;

    bool isVisible = dtid < totalMeshletCount;

    if (isVisible)
    {
        isVisible = !IsCulled(meshletIndex, instanceIndex);

        const joint::Meshlet meshlet = g_Meshlets[meshletIndex];
        InterlockedAddToStat(joint::ReadbackStat::Geometry_TotalMeshletCount, 1);
        InterlockedAddToStat(joint::ReadbackStat::Geometry_TotalMeshletVertexCount, meshlet.VertexCount);
        InterlockedAddToStat(joint::ReadbackStat::Geometry_TotalMeshletTriangleCount, meshlet.TriangleCount);
    }

    if (isVisible)
    {
        const uint index = WavePrefixCountBits(isVisible);
        g_MeshPayload.m_MeshletIndices[index] = meshletIndex;
        g_MeshPayload.m_InstanceIndices[index] = instanceIndex;

        const joint::Meshlet meshlet = g_Meshlets[meshletIndex];
        InterlockedAddToStat(joint::ReadbackStat::Geometry_MeshletCount, 1);
        InterlockedAddToStat(joint::ReadbackStat::Geometry_MeshletVertexCount, meshlet.VertexCount);
        InterlockedAddToStat(joint::ReadbackStat::Geometry_MeshletTriangleCount, meshlet.TriangleCount);
    }

    const uint visibleCount = WaveActiveCountBits(isVisible);
    DispatchMesh(visibleCount, 1, 1, g_MeshPayload);
}

[NumThreads((uint)joint::MeshletConsts::MsGroupSize, 1, 1)]
[OutputTopology("triangle")]
void MsMain(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload MeshPayload payload,
    out vertices VsOutput outVertices[(uint)joint::MeshletConsts::MaxVertexCount],
    out indices uint3 outTriangles[(uint)joint::MeshletConsts::MaxTriangleCount]
)
{
    InterlockedAddToStat(joint::ReadbackStat::Geometry_MsInvocationCount, 1);

    const uint totalMeshletCount = BenzinGetRootConstant(joint::GeometryResources::TotalMeshletCount);

    const uint meshletIndex = payload.m_MeshletIndices[gid];
    const uint instanceIndex = payload.m_InstanceIndices[gid];

    if (meshletIndex * instanceIndex >= totalMeshletCount)
        return;

    const joint::Meshlet meshlet = g_Meshlets[meshletIndex];
    SetMeshOutputCounts(meshlet.VertexCount, meshlet.TriangleCount);

    if (gtid < meshlet.VertexCount)
    {
        const uint vertexIndex = g_MeshletIndirectVertices[meshlet.VertexOffset + gtid];
        const joint::MeshVertex vertex = g_Vertices[vertexIndex];

        outVertices[gtid] = ProcessVertex(vertex, meshletIndex, instanceIndex);
    }

    if (gtid < meshlet.TriangleCount)
    {
        const uint triangleIndex = meshlet.IndexOffset + gtid * 3;
        const uint3 indices = uint3(g_MeshletIndices[triangleIndex + 0], g_MeshletIndices[triangleIndex + 1], g_MeshletIndices[triangleIndex + 2]);

        outTriangles[gtid] = indices;
    }
}
#endif

// Must match with joint::MeshVertex
struct VsInput
{
    float3 m_Position : Position;
    float3 m_Normal : Normal;
    float2 m_Uv : Uv;
};

VsOutput VsMain(VsInput vertex, uint vertexIndex : SV_VertexID)
{
    // InterlockedAddToStat(joint::ReadbackStat::Geometry_VsInvocationCount, 1);

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

#if 0
struct ScreenBounds2
{
    float2 m_UvMin;
    float2 m_UvMax;
    float m_NearestDepth;
};

ScreenBounds2 CalcScreenBounds2(float3 worldCenter, float worldRadius)
{
    ScreenBounds2 bounds;
    bounds.m_UvMin = 1.0;
    bounds.m_UvMax = 0.0;
    bounds.m_NearestDepth = 0.0;

    const float3 worldOffsets[8] =
    {
        float3(-worldRadius, -worldRadius, -worldRadius),
        float3(-worldRadius, -worldRadius, +worldRadius),
        float3(-worldRadius, +worldRadius, -worldRadius),
        float3(-worldRadius, +worldRadius, +worldRadius),
        float3(+worldRadius, -worldRadius, -worldRadius),
        float3(+worldRadius, -worldRadius, +worldRadius),
        float3(+worldRadius, +worldRadius, -worldRadius),
        float3(+worldRadius, +worldRadius, +worldRadius),
    };

    [unroll]
    for (uint i = 0; i < 8; ++i)
    {
        const float3 worldCorner = worldCenter + worldOffsets[i];
        const float4 clipCorner = mul(float4(worldCorner, 1.0), GetCameraConsts().WorldToClip);

        const float3 ndcCorner = clipCorner.xyz / clipCorner.w;

        float2 uvCorner = ndcCorner.xy * 0.5 + 0.5;
        uvCorner.y = 1.0 - uvCorner.y;

        bounds.m_UvMin = min(bounds.m_UvMin, uvCorner);
        bounds.m_UvMax = max(bounds.m_UvMax, uvCorner);
        bounds.m_NearestDepth = max(bounds.m_NearestDepth, ndcCorner.z); // Reversed depth buffer -> max
    }

    return bounds;
}

struct ScreenRect
{
    float2 m_Min;
    float2 m_Max;
    float m_NearestDepth;
    bool m_IsValid;
};

ScreenRect ProjectSphereToScreen(float3 worldCenter, float worldRadius)
{
    ScreenRect rect;
    rect.m_IsValid = false;

    float4 clipCenter = mul(float4(worldCenter, 1.0), GetCameraConsts().WorldToClip);
    if (clipCenter.w <= 0.0) // sphere behind camera
        return rect;

    float3 ndcCenter = clipCenter.xyz / clipCenter.w;

    float3 offsetX = worldCenter + float3(worldRadius, 0, 0);
    float3 offsetY = worldCenter + float3(0, worldRadius, 0);

    float4 clipX = mul(float4(offsetX, 1.0), GetCameraConsts().WorldToClip);
    float4 clipY = mul(float4(offsetY, 1.0), GetCameraConsts().WorldToClip);

    float2 ndcX = clipX.xy / clipX.w;
    float2 ndcY = clipY.xy / clipY.w;

    float2 ndcRadius;
    ndcRadius.x = abs(ndcX.x - ndcCenter.x);
    ndcRadius.y = abs(ndcY.y - ndcCenter.y);

    float r = max(ndcRadius.x, ndcRadius.y);

    // Make NDC bounding rect
    float2 minNDC = ndcCenter.xy - r;
    float2 maxNDC = ndcCenter.xy + r;

    // Clamp to screen [-1,1]
    minNDC = max(minNDC, float2(-1.0, -1.0));
    maxNDC = min(maxNDC, float2( 1.0,  1.0));

    if (minNDC.x >= maxNDC.x || minNDC.y >= maxNDC.y)
        return rect; // degenerate / off-screen

    rect.m_Min = (minNDC * 0.5f + 0.5f);
    rect.m_Max = (maxNDC * 0.5f + 0.5f);

    rect.m_Min.y = 1.0 - rect.m_Min.y;
    rect.m_Max.y = 1.0 - rect.m_Max.y;

    float tmp = rect.m_Min.y;
    rect.m_Min.y = rect.m_Max.y;
    rect.m_Max.y = tmp;

     // --- Nearest depth in NDC ---
    float3 viewCenter = mul(float4(worldCenter,1.0), GetCameraConsts().WorldToView).xyz;
    float nearestViewZ = viewCenter.z - worldRadius;
    nearestViewZ = max(nearestViewZ, GetCameraConsts().NearPlane);

    float4 clipNearest = mul(float4(0 , 0, nearestViewZ, 1), GetCameraConsts().ViewToClip);
    rect.m_NearestDepth = clipNearest.z / clipNearest.w; // NDC depth [-1,1]

    rect.m_IsValid = true;

    return rect;
}

float3 MeshletDebugColor(uint meshletIndex)
{
    const float3 palette[12] =
    {
        float3(1.0, 0.0, 0.0),   // red
        float3(0.0, 1.0, 0.0),   // green
        float3(0.0, 0.0, 1.0),   // blue
        float3(1.0, 1.0, 0.0),   // yellow
        float3(1.0, 0.0, 1.0),   // magenta
        float3(0.0, 1.0, 1.0),   // cyan
        float3(1.0, 0.5, 0.0),   // orange
        float3(0.5, 0.0, 1.0),   // purple
        float3(0.5, 1.0, 0.0),   // lime
        float3(0.0, 0.5, 1.0),   // sky blue
        float3(1.0, 0.0, 0.5),   // pink
        float3(0.0, 1.0, 0.5),   // aqua green
    };

    return palette[meshletIndex % 12];
}
#endif

PackedGBuffer PsMain(VsOutput input)
{
    InterlockedAddToStat(joint::ReadbackStat::Geometry_PsInvocationCount, 1);
    
    const uint drawPartIndex = BenzinGetRootConstant(joint::GeometryResources::MeshDrawPartIndex);
    const uint materialIndex = g_MeshDrawParts[drawPartIndex].m_MaterialIndex;
    const joint::Material material = g_Materials[materialIndex];

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

#if 0
    gbuffer.Albedo = float3(input.m_Uv, 0.0);
    gbuffer.Albedo = MeshletDebugColor(input.m_PrimitiveIndex);
#else
    gbuffer.Roughness = material.m_RoughnessFactor;
    gbuffer.Emissive = material.m_EmissiveFactor;
    gbuffer.Metallic = material.m_MetalnessFactor;
#endif

    if (material.m_NormalTextureHeapIndex != g_MaxU32)
    {
        Texture2D<float4> normalTexture = ResourceDescriptorHeap[material.m_NormalTextureHeapIndex];

        float3 normalSample = normalTexture.Sample(g_LinearWrapSampler, input.m_Uv).xyz;
        normalSample = 2.0 * normalSample - 1.0;
        normalSample.xy *= material.m_NormalScale;
        normalSample = normalize(normalSample);

        const float3 worldViewVector = normalize(GetCameraConsts().WorldPosition - input.m_WorldPosition);
        const float3x3 tbn = CotangentFrame(gbuffer.WorldNormal, -worldViewVector, input.m_Uv);

        gbuffer.WorldNormal = normalize(mul(normalSample, tbn));
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
