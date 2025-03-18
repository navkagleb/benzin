#include "joint/geometry_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "gbuffer.hlsli"
#include "joint/mesh_types.hpp"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(StructuredBuffer<joint::MeshTransform>, g_MeshTransforms, joint::GeometryResources::MeshTransforms);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshVertex>, g_MeshVertices, joint::GeometryResources::MeshVertices);
BenzinDeclareRootResource(StructuredBuffer<uint>, g_MeshIndices, joint::GeometryResources::MeshIndices);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshInfo>, g_SubMeshInfos, joint::GeometryResources::SubMeshInfos);
BenzinDeclareRootResource(StructuredBuffer<joint::MeshInstance>, g_SubMeshInstances, joint::GeometryResources::SubMeshInstances);
BenzinDeclareRootResource(StructuredBuffer<joint::Material>, g_Materials, joint::GeometryResources::Materials);

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

joint::MeshInstance GetMeshInstance()
{
    return g_SubMeshInstances[BenzinGetRootConstant(joint::GeometryResources::SubMeshInstanceIndex)];
}

joint::MeshVertex GetMeshVertex(uint indexIndex, uint subMeshIndex)
{
    const joint::MeshInfo meshInfo = g_SubMeshInfos[subMeshIndex];

    const uint vertexIndex = g_MeshIndices[meshInfo.IndexOffset + indexIndex];
    const joint::MeshVertex vertex = g_MeshVertices[meshInfo.VertexOffset + vertexIndex];

    return vertex;
}

struct VsOutput
{
    float4 ClipPosition : SV_Position;
    float3 WorldPosition : WorldPosition;
    float3 ViewPosition : ViewPosition;
    float3 PrevViewPosition : PrevViewPosition;
    float3 WorldNormal : WorldNormal;
    float2 Uv : Uv;
};

VsOutput VsMain(uint indexIndex : SV_VertexID)
{
    const joint::MeshInstance meshInstance = GetMeshInstance();
    const joint::MeshVertex vertex = GetMeshVertex(indexIndex, meshInstance.SubMeshIndex);
    const joint::MeshTransform transform = g_MeshTransforms[BenzinGetRootConstant(joint::GeometryResources::MeshTransformIndex)];

    const joint::CameraConsts camera = g_FrameConstants.Camera;

    const float4 objectPosition = mul(float4(vertex.Position, 1.0f), meshInstance.Transform);
    const float3 objectNormal = mul(vertex.Normal, (float3x3)meshInstance.Transform);

    const float4 worldPosition = mul(objectPosition, transform.LocalToWorld);
    const float4 prevWorldPosition = mul(objectPosition, transform.PrevLocalToWorld);
    const float3 worldNormal = mul(objectNormal, (float3x3)transform.LocalToWorld); // TODO: Maybe I still need to yse 'WorldMatrixForNormals'?

    const float4 viewPosition = mul(worldPosition, camera.WorldToView);

    VsOutput output = (VsOutput)0;
    output.ClipPosition = mul(worldPosition, camera.WorldToClip);
    output.WorldPosition = worldPosition.xyz;
    output.ViewPosition = viewPosition.xyz;
    output.PrevViewPosition = mul(prevWorldPosition, g_FrameConstants.PrevCamera.WorldToView).xyz;
    output.WorldNormal = worldNormal;
    output.Uv = vertex.Uv;

    return output;
}

struct PsOutput
{
    float4 Color0 : SV_Target0;
    float4 Color1 : SV_Target1;
    float4 Color2 : SV_Target2;
    float4 Color3 : SV_Target3;
    float4 Color4 : SV_Target4;
};

PsOutput PsMain(VsOutput input)
{
    const joint::MeshInstance meshInstance = GetMeshInstance();
    const joint::Material material = g_Materials[meshInstance.MaterialIndex];

    UnpackedGBuffer gbuffer;
    gbuffer.Albedo = material.AlbedoFactor.rgb;
    gbuffer.Roughness = material.RoughnessFactor;
    gbuffer.Emissive = material.EmissiveFactor;
    gbuffer.Metallic = material.MetalnessFactor;
    gbuffer.WorldNormal = normalize(input.WorldNormal);

    if (material.AlbedoTextureIndex != g_InvalidIndex)
    {
        Texture2D<float4> albedoTexture = ResourceDescriptorHeap[material.AlbedoTextureIndex];
        const float4 albedoSample = albedoTexture.Sample(g_LinearWrapSampler, input.Uv);

        if (albedoSample.a < material.AlphaCutoff)
        {
            discard;
        }

        gbuffer.Albedo *= albedoSample.rgb;
    }

    if (material.NormalTextureIndex != g_InvalidIndex)
    {
        const joint::CameraConsts cameraConstants = g_FrameConstants.Camera;

        Texture2D<float4> normalTexture = ResourceDescriptorHeap[material.NormalTextureIndex];

        float3 normalSample = normalTexture.Sample(g_LinearWrapSampler, input.Uv).xyz;
        normalSample = 2.0 * normalSample - 1.0;
        normalSample = normalize(normalSample * float3(material.NormalScale, material.NormalScale, 1.0));
        normalSample = ExpandNormal(normalSample.xy);

        const float3 worldViewDirection = normalize(cameraConstants.WorldPosition - input.WorldPosition);

        const float3x3 tbn = CotangentFrame(gbuffer.WorldNormal, -worldViewDirection, input.Uv);
        //const float3x3 tbn = GetTBNBasis(worldPosition, input.WorldNormal, input.TexCoord);

        gbuffer.WorldNormal = normalize(mul(normalSample, tbn));
    }

    if (material.EmissiveTextureIndex != g_InvalidIndex)
    {
        Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[material.EmissiveTextureIndex];
        const float3 emissiveSample = emissiveTexture.Sample(g_LinearWrapSampler, input.Uv).rgb;

        gbuffer.Emissive *= emissiveSample;
    }

    if (material.MetallicRoughnessTextureIndex != g_InvalidIndex)
    {
        Texture2D<float4> metallicRoughnessTexture = ResourceDescriptorHeap[material.MetallicRoughnessTextureIndex];
        const float metallicSample = metallicRoughnessTexture.Sample(g_LinearWrapSampler, input.Uv).b;
        const float roughnessSample = metallicRoughnessTexture.Sample(g_LinearWrapSampler, input.Uv).g;

        gbuffer.Metallic *= metallicSample;
        gbuffer.Roughness *= roughnessSample;
    }

    {
        const float4 clipPosition = mul(float4(input.ViewPosition, 1.0), g_FrameConstants.Camera.ViewToClip);
        const float4 prevClipPosition = mul(float4(input.PrevViewPosition, 1.0), g_FrameConstants.PrevCamera.ViewToClip);

        // const float2 uv = input.ClipPosition.xy * g_FrameConstants.InvRenderResolution;
        const float2 uv = ClipToUv(clipPosition);
        const float2 prevUv = ClipToUv(prevClipPosition);

        gbuffer.UvMv = (uv - prevUv) * g_FrameConstants.RenderResolution; // TODO: Pack/Unpack Mv
        gbuffer.ViewDepthMv = input.ViewPosition.z - input.PrevViewPosition.z;

        gbuffer.ViewDepth = input.ViewPosition.z;
    }

    // gbuffer.Albedo = 0.6;

    const PackedGBuffer packedGBuffer = PackGBuffer(gbuffer);

    PsOutput output = (PsOutput)0;
    output.Color0 = packedGBuffer.Color0;
    output.Color1 = packedGBuffer.Color1;
    output.Color2 = packedGBuffer.Color2;
    output.Color3 = packedGBuffer.Color3;
    output.Color4 = packedGBuffer.Color4;

    return output;
}
