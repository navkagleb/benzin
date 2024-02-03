#include "common.hlsli"
#include "gbuffer.hlsli"

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

static joint::MeshNode g_IdentityMeshNode = { g_IdentityMatrix, g_IdentityMatrix };

joint::MeshInstance FetchMeshInstance()
{
    StructuredBuffer<joint::MeshInstance> meshInstanceBuffer = ResourceDescriptorHeap[GetRootConstant(joint::GeometryPassRC_MeshInstanceBuffer)];
    const uint meshInstanceIndex = GetRootConstant(joint::GeometryPassRC_MeshInstanceIndex);

    return meshInstanceBuffer[meshInstanceIndex];
}

joint::MeshVertex FetchVertex(uint indexIndex, uint meshIndex)
{
    StructuredBuffer<joint::MeshVertex> vertexBuffer = ResourceDescriptorHeap[GetRootConstant(joint::GeometryPassRC_MeshVertexBuffer)];
    Buffer<uint> indexBuffer = ResourceDescriptorHeap[GetRootConstant(joint::GeometryPassRC_MeshIndexBuffer)];
    StructuredBuffer<joint::MeshInfo> meshInfoBuffer = ResourceDescriptorHeap[GetRootConstant(joint::GeometryPassRC_MeshInfoBuffer)];

    const joint::MeshInfo meshInfo = meshInfoBuffer[meshIndex];

    const uint vertexIndex = indexBuffer[meshInfo.IndexOffset + indexIndex];
    const joint::MeshVertex vertex = vertexBuffer[meshInfo.VertexOffset + vertexIndex];

    return vertex;
}

joint::MeshNode FetchMeshNode(uint meshNodeIndex)
{
    const uint meshNodeBufferIndex = GetRootConstant(joint::GeometryPassRC_MeshNodeBuffer);

    if (meshNodeBufferIndex == g_InvalidIndex || meshNodeIndex == g_InvalidIndex)
    {
        return g_IdentityMeshNode;
    }

    StructuredBuffer<joint::MeshNode> meshNodeBuffer = ResourceDescriptorHeap[meshNodeBufferIndex];
    return meshNodeBuffer[meshNodeIndex];
}

joint::Transform FetchTransform()
{
    ConstantBuffer<joint::Transform> transformBuffer = ResourceDescriptorHeap[GetRootConstant(joint::GeometryPassRC_TransformBuffer)];
    return transformBuffer;
}

joint::Material FetchMaterial(uint materialIndex)
{
    StructuredBuffer<joint::Material> materialBuffer = ResourceDescriptorHeap[GetRootConstant(joint::GeometryPassRC_MaterialBuffer)];
    return materialBuffer[materialIndex];
}

struct VS_Output
{
    float4 HomogeneousPosition : SV_Position;
    float3 WorldPosition : WorldPosition;
    float3 WorldNormal : WorldNormal;
    float2 UV : UV;
};

VS_Output VS_Main(uint indexIndex : SV_VertexID)
{
    const joint::CameraConstants cameraConstants = FetchCameraConstants();

    const joint::MeshInstance meshInstance = FetchMeshInstance();
    const joint::MeshVertex vertex = FetchVertex(indexIndex, meshInstance.MeshIndex);
    const joint::MeshNode meshNode = FetchMeshNode(meshInstance.NodeIndex);
    const joint::Transform transform = FetchTransform();
    
    // #TODO: Maybe can joint meshNode.WorldTransform with transform.WorldMatrix?
    const float4 worldPosition = mul(float4(vertex.Position, 1.0f), meshNode.WorldTransform);
    const float3 worldNormal = mul(vertex.Normal, (float3x3)transpose(meshNode.InverseWorldTransform));

    const float4 worldPosition1 = mul(worldPosition, transform.WorldMatrix);
    const float3 worldNormal1 = mul(worldNormal, (float3x3)transpose(transform.InverseWorldMatrix));

    VS_Output output = (VS_Output)0;
    output.HomogeneousPosition = mul(worldPosition1, cameraConstants.ViewProjection);
    output.WorldPosition = worldPosition1.xyz;
    output.WorldNormal = worldNormal1;
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
    const joint::CameraConstants cameraConstants = FetchCameraConstants();

    const joint::MeshInstance meshInstance = FetchMeshInstance();
    const joint::Material material = FetchMaterial(meshInstance.MaterialIndex);

    UnpackedGBuffer gbuffer;
    gbuffer.Albedo = material.AlbedoFactor;
    gbuffer.WorldNormal = normalize(input.WorldNormal);
    gbuffer.Emissive = material.EmissiveFactor;
    gbuffer.Roughness = material.RoughnessFactor;
    gbuffer.Metalness = material.MetalnessFactor;

    if (material.AlbedoTextureIndex != g_InvalidIndex)
    {
        Texture2D<float4> albedoTexture = ResourceDescriptorHeap[material.AlbedoTextureIndex];
        const float4 albedoSample = albedoTexture.Sample(g_LinearWrapSampler, input.UV);

        if (albedoSample.a < material.AlphaCutoff)
        {
            discard;
        }

        gbuffer.Albedo *= albedoSample;
    }

    if (material.NormalTextureIndex != g_InvalidIndex)
    {
        Texture2D<float4> normalTexture = ResourceDescriptorHeap[material.NormalTextureIndex];

        float3 normalSample = normalTexture.Sample(g_LinearWrapSampler, input.UV).xyz;
        normalSample = 2.0 * normalSample - 1.0;
        normalSample = normalize(normalSample * float3(material.NormalScale, material.NormalScale, 1.0));
        normalSample = ExpandNormal(normalSample.xy);

        const float3 worldViewDirection = normalize(cameraConstants.WorldPosition - input.WorldPosition);

        const float3x3 tbn = CotangentFrame(gbuffer.WorldNormal, -worldViewDirection, input.UV);
        //const float3x3 tbn = GetTBNBasis(worldPosition, input.WorldNormal, input.TexCoord);

        gbuffer.WorldNormal = normalize(mul(normalSample, tbn));
    }

    if (material.EmissiveTextureIndex != g_InvalidIndex)
    {
        Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[material.EmissiveTextureIndex];
        const float3 emissiveSample = emissiveTexture.Sample(g_LinearWrapSampler, input.UV).rgb;

        gbuffer.Emissive *= emissiveSample;
    }

    if (material.MetalRoughnessTextureIndex != g_InvalidIndex)
    {
        Texture2D<float4> metalRoughnessTexture = ResourceDescriptorHeap[material.MetalRoughnessTextureIndex];
        const float roughnessSample = metalRoughnessTexture.Sample(g_LinearWrapSampler, input.UV).g;
        const float metalnessSample = metalRoughnessTexture.Sample(g_LinearWrapSampler, input.UV).b;

        gbuffer.Roughness *= roughnessSample;
        gbuffer.Metalness *= metalnessSample;
    }

    PackedGBuffer packedGBuffer = PackGBuffer(gbuffer);

    PS_Output output = (PS_Output)0;
    output.Color0 = packedGBuffer.Color0;
    output.Color1 = packedGBuffer.Color1;
    output.Color2 = packedGBuffer.Color2;
    output.Color3 = packedGBuffer.Color3;

    return output;
}
