#include "joint/geometry_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "gbuffer.hlsli"
#include "joint/mesh_types.hpp"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(StructuredBuffer<joint::EntityTransform>, g_EntityTransforms, joint::GeometryResources::EntityTransforms);
BenzinDeclareRootResource(StructuredBuffer<joint::Material>, g_UnifiedMaterials, joint::GeometryResources::UnifiedMaterials);
BenzinDeclareRootResource(StructuredBuffer<float4x4>, g_ObjectToLocalMatrices, joint::GeometryResources::ObjectToLocalMatrices);

BenzinDeclareRootResource(StructuredBuffer<joint::MeshVertex>, g_Vertices, joint::GeometryResources::Vertices);
BenzinDeclareRootResource(StructuredBuffer<joint::Meshlet>, g_Meshlets, joint::GeometryResources::Meshlets);
BenzinDeclareRootResource(Buffer<uint>, g_MeshletIndirectVertices, joint::GeometryResources::MeshletIndirectVertices);
BenzinDeclareRootResource(Buffer<uint>, g_MeshletIndices, joint::GeometryResources::MeshletIndices); // uint8_t

static const uint g_AsGroupSize = (uint)joint::MeshletConsts::AsGroupSize;

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

// Must match with joint::MeshVertex
struct VsInput
{
    float3 Position : Position;
    float3 Normal : Normal;
    float2 Uv : Uv;
};

struct VsOutput
{
    float4 ClipPosition : SV_Position;
    float3 WorldPosition : WorldPosition;
    float ViewDepth : ViewDepth;
    float3 PrevViewPosition : PrevViewPosition;
    float3 WorldNormal : WorldNormal;
    float2 Uv : Uv;
};

struct MeshPayload
{
    uint MeshletIndices[g_AsGroupSize];
};

    const float3 worldNormal = mul(objectNormal, (float3x3)entityTransform.LocalToWorld); // TODO: Maybe I still need to use 'WorldMatrixForNormals'?
float4x4 GetObjectToLocal()
{
    return g_ObjectToLocalMatrices[BenzinGetRootConstant(joint::GeometryResources::ObjectToLocalMatrixIndex)];
}

joint::EntityTransform GetEntityTransform()
{
    return g_EntityTransforms[BenzinGetRootConstant(joint::GeometryResources::EntityTransformIndex)];
}

VsOutput ProcessVertex(joint::MeshVertex vertex, uint meshletIndex)
    const float4x4 objectToLocal = GetObjectToLocal();
    const float4 localPosition = mul(float4(vertex.Position, 1.0), objectToLocal);
    const float3 localNormal = mul(vertex.Normal, (float3x3)objectToLocal);

    const joint::EntityTransform entityTransform = GetEntityTransform();
    const float4 worldPosition = mul(localPosition, entityTransform.LocalToWorld);
    const float4 prevWorldPosition = mul(localPosition, entityTransform.PrevLocalToWorld);
    const float3 worldNormal = mul(localNormal, (float3x3)entityTransform.LocalToWorld); // TODO: Maybe I still need to use 'WorldMatrixForNormals'?

    const float4 viewPosition = mul(worldPosition, GetCameraConsts().WorldToView);

    VsOutput output = (VsOutput)0;
    output.ClipPosition = mul(worldPosition, GetCameraConsts().WorldToClip);
    output.WorldPosition = worldPosition.xyz;
    output.ViewDepth = viewPosition.z;
    output.PrevViewPosition = mul(prevWorldPosition, GetPrevCameraConsts().WorldToView).xyz;
    output.WorldNormal = worldNormal;
    output.Uv = vertex.Uv;

    return output;
}

groupshared MeshPayload g_MeshPayload;

[NumThreads(g_AsGroupSize, 1, 1)]
void AsMain(uint dtid : SV_DispatchThreadID)
{
    bool isVisible = false;

    const uint meshletCount = BenzinGetRootConstant(joint::GeometryResources::MeshletCount);
    if (dtid < meshletCount)
    {
        isVisible = true;
    }

    if (isVisible)
    {
        const uint index = WavePrefixCountBits(isVisible);
        g_MeshPayload.MeshletIndices[index] = dtid;
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
    const uint meshletCount = BenzinGetRootConstant(joint::GeometryResources::MeshletCount);
    const uint meshletIndex = payload.MeshletIndices[gid];

    if (meshletIndex >= meshletCount)
    {
        return;
    }

    const joint::Meshlet meshlet = g_Meshlets[meshletIndex];

    SetMeshOutputCounts(meshlet.VertexCount, meshlet.TriangleCount);

    if (gtid < meshlet.VertexCount)
    {
        const uint vertexIndex = g_MeshletIndirectVertices[meshlet.VertexOffset + gtid];
        const joint::MeshVertex vertex = g_Vertices[vertexIndex];

        outVertices[gtid] = ProcessVertex(vertex);
    }

    if (gtid < meshlet.TriangleCount)
    {
        const uint triangleIndex = meshlet.IndexOffset + gtid * 3;

        const uint3 indices = uint3(
            g_MeshletIndices[triangleIndex + 0],
            g_MeshletIndices[triangleIndex + 1],
            g_MeshletIndices[triangleIndex + 2]
        );

        outTriangles[gtid] = indices;
    }
}

VsOutput VsMain(VsInput vertex)
{
    return ProcessVertex((joint::MeshVertex)vertex);
}

#if !defined(IS_DEPTH_PREPASS)
PackedGBuffer PsMain(VsOutput input)
#else
void PsMain(VsOutput input)
#endif
{
    const joint::Material material = g_UnifiedMaterials[BenzinGetRootConstant(joint::GeometryResources::MaterialIndex)];

    float3 albedo = material.AlbedoFactor.rgb;
    if (material.AlbedoTextureHeapIndex != g_InvalidIndex)
    {
        Texture2D<float4> albedoTexture = ResourceDescriptorHeap[material.AlbedoTextureHeapIndex];
        const float4 albedoSample = albedoTexture.Sample(g_LinearWrapSampler, input.Uv);

#if defined(IS_ALPHA_TEST_ENABLED)
        if (albedoSample.a < material.AlphaCutoff)
        {
            discard;
        }
#endif

        albedo *= albedoSample.rgb;
    }

#if !defined(IS_DEPTH_PREPASS)
    GBuffer gbuffer;
    gbuffer.Albedo = albedo;
    gbuffer.Roughness = material.RoughnessFactor;
    gbuffer.Emissive = material.EmissiveFactor;
    gbuffer.Metallic = material.MetalnessFactor;
    gbuffer.WorldNormal = normalize(input.WorldNormal);
    gbuffer.ViewDepth = input.ViewDepth;

    if (material.NormalTextureHeapIndex != g_InvalidIndex)
    {
        Texture2D<float4> normalTexture = ResourceDescriptorHeap[material.NormalTextureHeapIndex];

        float3 normalSample = normalTexture.Sample(g_LinearWrapSampler, input.Uv).xyz;
        normalSample = 2.0 * normalSample - 1.0;
        normalSample = normalize(normalSample * float3(material.NormalScale, material.NormalScale, 1.0));
        normalSample = ExpandNormal(normalSample.xy);

        const float3 worldViewDirection = normalize(GetCameraConsts().WorldPosition - input.WorldPosition);

        const float3x3 tbn = CotangentFrame(gbuffer.WorldNormal, -worldViewDirection, input.Uv);
        //const float3x3 tbn = GetTBNBasis(worldPosition, input.WorldNormal, input.TexCoord);

        gbuffer.WorldNormal = normalize(mul(normalSample, tbn));
    }

    if (material.EmissiveTextureHeapIndex != g_InvalidIndex)
    {
        Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[material.EmissiveTextureHeapIndex];
        const float3 emissiveSample = emissiveTexture.Sample(g_LinearWrapSampler, input.Uv).rgb;

        gbuffer.Emissive *= emissiveSample;
    }

    if (material.MetallicRoughnessTextureHeapIndex != g_InvalidIndex)
    {
        Texture2D<float4> metallicRoughnessTexture = ResourceDescriptorHeap[material.MetallicRoughnessTextureHeapIndex];
        const float metallicSample = metallicRoughnessTexture.Sample(g_LinearWrapSampler, input.Uv).b;
        const float roughnessSample = metallicRoughnessTexture.Sample(g_LinearWrapSampler, input.Uv).g;

        gbuffer.Metallic *= metallicSample;
        gbuffer.Roughness *= roughnessSample;
    }

    CalcGBufferMv(input.ClipPosition.xy, input.ViewDepth, input.PrevViewPosition, gbuffer);

    return PackGBuffer(gbuffer);
#endif
}
