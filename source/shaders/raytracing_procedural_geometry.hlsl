#include "common.hlsli"
#include "rt_geometry_generator.hlsli"

static const uint g_MaxRayRecursionDepth = 5;
static const float g_InShadowIrradiance = 0.35f;
static const float4 g_BackgroundColor = float4(0.2f, 0.2f, 0.2f, 1.0f);

enum RootConstant : uint32_t
{
    g_TopLevelASIndex,
    g_SceneConstantBufferIndex,
    g_TriangleVertexBufferIndex,
    g_TriangleIndexBufferIndex,
    g_MeshMaterialBufferIndex,
    g_ProceduralGeometryBufferIndex,
    g_RaytracingOutputTextureIndex,
};

struct Vertex
{
    float3 Position;
    float3 Normal;
    float3 UV;
};

struct SceneConstants
{
    float4x4 InverseViewProjectionMatrix;
    float4 CameraPosition;
    float4 LightPosition;
    float4 LightAmbientColor;
    float4 LightDiffuseColor;
    float Reflectance;
    float ElapsedTime;
};

struct MeshMaterial
{
    float4 Albedo;
    float ReflectanceCoefficient;
    float DiffuseCoefficient;
    float SpecularCoefficient;
    float SpecularPower;
    float StepScale;
};

struct ProceduralGeometryConstants
{
    float4x4 LocalSpaceToBottomLevelASMatrix;
    float4x4 BottomLevelASToLocalSpaceMatrix;
    uint MaterialIndex;
    uint AABBGeometryType;

    float2 __UnusedPadding;
};

struct RadianceRayPayload
{
    float4 Color;
    uint32_t RecursionDepth;
};

struct ShadowRayPayload
{
    bool IsHitted;
};

struct ProceduralAttributes
{
    float3 Normal;
};

// RayTracing helpers
namespace trace_ray_parameters
{

    static const uint g_InstanceMask = ~0; // Everything is visible.

    namespace hit_group
    {

        static const uint g_RadianceRayOffset = 0;
        static const uint g_ShadowRayOffset = 1;
        static const uint g_GeometryStride = 2; // 'RayType' count

    }

    namespace miss_shader
    {

        static const uint g_RadianceRayOffset = 0;
        static const uint g_ShadowRayOffset = 1;

    }

}

float3 GetWorldHitPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

float4 TraceRadianceRay(rt_common::Ray ray, uint currentRayRecursionDepth)
{
    if (currentRayRecursionDepth >= g_MaxRayRecursionDepth)
    {
        return 0.0f;
    }

    RayDesc rayDesc;
    rayDesc.Origin = ray.Origin;
    rayDesc.Direction = ray.Direction;
    rayDesc.TMin = 0.0f;
    rayDesc.TMax = 10000.0f;

    RadianceRayPayload payload;
    payload.Color = 0.0f;
    payload.RecursionDepth = currentRayRecursionDepth + 1;

    RaytracingAccelerationStructure topLevelAS = ResourceDescriptorHeap[GetRootConstant(g_TopLevelASIndex)];

    TraceRay(
        topLevelAS,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        trace_ray_parameters::g_InstanceMask,
        trace_ray_parameters::hit_group::g_RadianceRayOffset,
        trace_ray_parameters::hit_group::g_GeometryStride,
        trace_ray_parameters::miss_shader::g_RadianceRayOffset,
        rayDesc,
        payload
    );

    return payload.Color;
}

bool TraceShadowRay(rt_common::Ray ray, uint currentRayRecursionDepth)
{
    if (currentRayRecursionDepth >= g_MaxRayRecursionDepth)
    {
        return false;
    }

    RayDesc rayDesc;
    rayDesc.Origin = ray.Origin;
    rayDesc.Direction = ray.Direction;
    rayDesc.TMin = 0.0f;
    rayDesc.TMax = 10000.0f;

    // Initialize shadow ray payload.
    // Set the initial value to true since closest and any hit shaders are skipped. 
    // Shadow miss shader, if called, will set it to false.
    ShadowRayPayload payload;
    payload.IsHitted = true;

    RaytracingAccelerationStructure topLevelAS = ResourceDescriptorHeap[GetRootConstant(g_TopLevelASIndex)];

    TraceRay(
        topLevelAS,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
        RAY_FLAG_FORCE_OPAQUE | // Skip any hit shaders
        RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, // Skip closest hit shaders
        trace_ray_parameters::g_InstanceMask,
        trace_ray_parameters::hit_group::g_ShadowRayOffset,
        trace_ray_parameters::hit_group::g_GeometryStride,
        trace_ray_parameters::miss_shader::g_ShadowRayOffset,
        rayDesc,
        payload
    );

    return payload.IsHitted;
}

// Lighting
float CalculateDiffuseCoefficient(in float3 hitPosition, in float3 incidentLightRay, in float3 normal)
{
    float fNDotL = saturate(dot(-incidentLightRay, normal));
    return fNDotL;
}

float4 CalculateSpecularCoefficient(in float3 hitPosition, in float3 incidentLightRay, in float3 normal, in float specularPower)
{
    float3 reflectedLightRay = normalize(reflect(incidentLightRay, normal));
    return pow(saturate(dot(reflectedLightRay, normalize (-WorldRayDirection()))), specularPower);
}


float4 CalculatePhongLighting(in float4 albedo, in float3 normal, in bool isInShadow, in float diffuseCoef = 1.0, in float specularCoef = 1.0, in float specularPower = 50)
{
    const ConstantBuffer<SceneConstants> sceneConstants = ResourceDescriptorHeap[GetRootConstant(g_SceneConstantBufferIndex)];

    float3 hitPosition = GetWorldHitPosition();
    float3 lightPosition = sceneConstants.LightPosition.xyz;
    float shadowFactor = isInShadow ? g_InShadowIrradiance : 1.0;
    float3 incidentLightRay = normalize(hitPosition - lightPosition);

    // Diffuse component.
    float4 lightDiffuseColor = sceneConstants.LightDiffuseColor;
    float Kd = CalculateDiffuseCoefficient(hitPosition, incidentLightRay, normal);
    float4 diffuseColor = shadowFactor * diffuseCoef * Kd * lightDiffuseColor * albedo;

    // Specular component.
    float4 specularColor = float4(0, 0, 0, 0);
    if (!isInShadow)
    {
        float4 lightSpecularColor = float4(1, 1, 1, 1);
        float4 Ks = CalculateSpecularCoefficient(hitPosition, incidentLightRay, normal, specularPower);
        specularColor = specularCoef * Ks * lightSpecularColor;
    }

    // Ambient component.
    // Fake AO: Darken faces with normal facing downwards/away from the sky a little bit.
    float4 ambientColor = sceneConstants.LightAmbientColor;
    float4 ambientColorMin = sceneConstants.LightAmbientColor - 0.1f;
    float4 ambientColorMax = sceneConstants.LightAmbientColor;
    float a = 1 - saturate(dot(normal, float3(0, -1, 0)));
    ambientColor = albedo * lerp(ambientColorMin, ambientColorMax, a);
    
    return ambientColor + diffuseColor + specularColor;
}

float3 FresnelReflectanceSchlick(float3 I, float3 N, float3 f0)
{
    const float cosi = saturate(dot(-I, N));
    return f0 + (1 - f0)*pow(1 - cosi, 5);
}

rt_common::Ray GenerateCameraRay(uint2 texelPosition, float3 cameraPosition, float4x4 inverseViewProjectionMatrix)
{
    const float2 texelCenter = texelPosition + 0.5f;

    float2 screenPosition = texelCenter / DispatchRaysDimensions().xy * 2.0f - 1.0f;
    screenPosition.y = -screenPosition.y; // Invert Y for DirectX-style coordinates

    float4 worldPosition = mul(float4(screenPosition, 0.0f, 1.0f), inverseViewProjectionMatrix);
    worldPosition.xyz /= worldPosition.w;

    rt_common::Ray cameraRay;
    cameraRay.Origin = cameraPosition.xyz;
    cameraRay.Direction = normalize(worldPosition.xyz - cameraRay.Origin);

    return cameraRay;
}

// Texture coordinates on a horizontal plane.
float2 TexCoords(in float3 position)
{
    return position.xz;
}

// Calculate ray differentials.
void CalculateRayDifferentials(out float2 ddx_uv, out float2 ddy_uv, in float2 uv, in float3 hitPosition, in float3 surfaceNormal, in float3 cameraPosition, in float4x4 projectionToWorld)
{
    // Compute ray differentials by intersecting the tangent plane to the  surface.
    rt_common::Ray ddx = GenerateCameraRay(DispatchRaysIndex().xy + uint2(1, 0), cameraPosition, projectionToWorld);
    rt_common::Ray ddy = GenerateCameraRay(DispatchRaysIndex().xy + uint2(0, 1), cameraPosition, projectionToWorld);

    // Compute ray differentials.
    float3 ddx_pos = ddx.Origin - ddx.Direction * dot(ddx.Origin - hitPosition, surfaceNormal) / dot(ddx.Direction, surfaceNormal);
    float3 ddy_pos = ddy.Origin - ddy.Direction * dot(ddy.Origin - hitPosition, surfaceNormal) / dot(ddy.Direction, surfaceNormal);

    // Calculate texture sampling footprint.
    ddx_uv = TexCoords(ddx_pos) - uv;
    ddy_uv = TexCoords(ddy_pos) - uv;
}

float CheckersTextureBoxFilter(in float2 uv, in float2 dpdx, in float2 dpdy, in uint ratio)
{
    float2 w = max(abs(dpdx), abs(dpdy));   // Filter kernel
    float2 a = uv + 0.5*w;
    float2 b = uv - 0.5*w;

    // Analytical integral (box filter).
    float2 i = (floor(a) + min(frac(a)*ratio, 1.0) -
        floor(b) - min(frac(b)*ratio, 1.0)) / (ratio*w);
    return (1.0 - i.x)*(1.0 - i.y);
}

// Return analytically integrated checkerboard texture (box filter).
float AnalyticalCheckersTexture(in float3 hitPosition, in float3 surfaceNormal, in float3 cameraPosition, in float4x4 projectionToWorld)
{
    float2 ddx_uv;
    float2 ddy_uv;
    float2 uv = TexCoords(hitPosition);

    CalculateRayDifferentials(ddx_uv, ddy_uv, uv, hitPosition, surfaceNormal, cameraPosition, projectionToWorld);
    return CheckersTextureBoxFilter(uv, ddx_uv, ddy_uv, 50);
}

[shader("raygeneration")]
void RayGen()
{
    const ConstantBuffer<SceneConstants> sceneConstants = ResourceDescriptorHeap[GetRootConstant(g_SceneConstantBufferIndex)];

    const rt_common::Ray ray = GenerateCameraRay(DispatchRaysIndex().xy, sceneConstants.CameraPosition.xyz, sceneConstants.InverseViewProjectionMatrix);
    const uint currentRecursionDepth = 0;

    const float4 color = TraceRadianceRay(ray, currentRecursionDepth);
    
    const RWTexture2D<float4> rayTracingOutputTexture = ResourceDescriptorHeap[GetRootConstant(g_RaytracingOutputTextureIndex)];
    rayTracingOutputTexture[DispatchRaysIndex().xy] = color;
}

[shader("closesthit")]
void Triangle_RadianceRay_ClosestHit(inout RadianceRayPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    const ConstantBuffer<SceneConstants> sceneConstants = ResourceDescriptorHeap[GetRootConstant(g_SceneConstantBufferIndex)];
    const StructuredBuffer<Vertex> vertexBuffer = ResourceDescriptorHeap[GetRootConstant(g_TriangleVertexBufferIndex)];
    const Buffer<uint> indexBuffer = ResourceDescriptorHeap[GetRootConstant(g_TriangleIndexBufferIndex)];
    const StructuredBuffer<MeshMaterial> meshMaterialBufer = ResourceDescriptorHeap[GetRootConstant(g_MeshMaterialBufferIndex)];

    const uint triangleIndexStride = 3;
    const uint indexOffset = PrimitiveIndex() * triangleIndexStride;

    const uint index = indexBuffer.Load(indexOffset);
    const Vertex vertex = vertexBuffer[index];
    const MeshMaterial meshMaterial = meshMaterialBufer[0]; // #TODO: Hardcoded for grid mesh

    // Shadow component.
    const float3 hitPosition = GetWorldHitPosition();
    const rt_common::Ray shadowRay = { hitPosition, normalize(sceneConstants.LightPosition.xyz - hitPosition) };

    const bool isShadowRayHitted = TraceShadowRay(shadowRay, payload.RecursionDepth);

    const float checkers = AnalyticalCheckersTexture(GetWorldHitPosition(), vertex.Normal, sceneConstants.CameraPosition.xyz, sceneConstants.InverseViewProjectionMatrix);

    // Reflected component.
    float4 reflectedColor = 0.0f;
    if (meshMaterial.ReflectanceCoefficient > 0.001f)
    {
        // Trace a reflection ray.
        rt_common::Ray reflectionRay;
        reflectionRay.Origin = GetWorldHitPosition();
        reflectionRay.Direction = reflect(WorldRayDirection(), vertex.Normal);

        const float4 reflectionColor = TraceRadianceRay(reflectionRay, payload.RecursionDepth);

        const float3 fresnelR = FresnelReflectanceSchlick(WorldRayDirection(), vertex.Normal, meshMaterial.Albedo.xyz);
        reflectedColor = meshMaterial.ReflectanceCoefficient * float4(fresnelR, 1.0f) * reflectionColor;
    }

    // Calculate final color.
    const float4 phongColor = CalculatePhongLighting(meshMaterial.Albedo, vertex.Normal, isShadowRayHitted, meshMaterial.DiffuseCoefficient, meshMaterial.SpecularCoefficient, meshMaterial.SpecularPower);
    float4 color = checkers * (phongColor + reflectedColor);

    // Apply visibility falloff.
    const float t = RayTCurrent();
    color = lerp(color, g_BackgroundColor, 1.0 - exp(-0.000002 * t * t * t));

    payload.Color = color;
}

[shader("closesthit")]
void AABB_RadianceRay_ClosestHit(inout RadianceRayPayload payload, ProceduralAttributes attrs)
{
    const ConstantBuffer<SceneConstants> sceneConstants = ResourceDescriptorHeap[GetRootConstant(g_SceneConstantBufferIndex)];

    const uint proceduralGeometryIndex = GeometryIndex();

    const StructuredBuffer<ProceduralGeometryConstants> proceduralGeometryBuffer = ResourceDescriptorHeap[GetRootConstant(g_ProceduralGeometryBufferIndex)];
    const StructuredBuffer<MeshMaterial> meshMaterialBufer = ResourceDescriptorHeap[GetRootConstant(g_MeshMaterialBufferIndex)];

    const ProceduralGeometryConstants proceduralGeometryConstants = proceduralGeometryBuffer[proceduralGeometryIndex];
    const MeshMaterial meshMaterial = meshMaterialBufer[proceduralGeometryConstants.MaterialIndex];

    // Shadow component
    const float3 hitPosition = GetWorldHitPosition();
    const rt_common::Ray shadowRay = { hitPosition, normalize(sceneConstants.LightPosition.xyz - hitPosition) };

    const bool isShadowRayHitted = TraceShadowRay(shadowRay, payload.RecursionDepth);

    float checkers = AnalyticalCheckersTexture(GetWorldHitPosition(), attrs.Normal, hitPosition, sceneConstants.InverseViewProjectionMatrix);
    checkers = 1.0f;

    // Reflected component
    float4 reflectedColor = 0.0f;
    if (meshMaterial.ReflectanceCoefficient > 0.001f)
    {
        // Trace a reflection ray.
        rt_common::Ray reflectionRay;
        reflectionRay.Origin = GetWorldHitPosition();
        reflectionRay.Direction = reflect(WorldRayDirection(), attrs.Normal);

        const float4 reflectionColor = TraceRadianceRay(reflectionRay, payload.RecursionDepth);

        const float3 fresnelR = FresnelReflectanceSchlick(WorldRayDirection(), attrs.Normal, meshMaterial.Albedo.xyz);
        reflectedColor = meshMaterial.ReflectanceCoefficient * float4(fresnelR, 1.0f) * reflectionColor;
    }

    // Calculate final color
    const float4 phongColor = CalculatePhongLighting(meshMaterial.Albedo, attrs.Normal, isShadowRayHitted, meshMaterial.DiffuseCoefficient, meshMaterial.SpecularCoefficient, meshMaterial.SpecularPower);
    float4 color = checkers * (phongColor + reflectedColor);

    payload.Color = color;
}

[shader("intersection")]
void AnalyticAABB_Intersection()
{
    const uint proceduralGeometryIndex = GeometryIndex();

    const ConstantBuffer<SceneConstants> sceneConstants = ResourceDescriptorHeap[GetRootConstant(g_SceneConstantBufferIndex)];
    const StructuredBuffer<ProceduralGeometryConstants> proceduralGeometryBuffer = ResourceDescriptorHeap[GetRootConstant(g_ProceduralGeometryBufferIndex)];
    const ProceduralGeometryConstants proceduralGeometryConstants = proceduralGeometryBuffer[proceduralGeometryIndex];

    const rt_common::Ray localRay = rt_common::TransformRay(rt_common::GetObjectRay(), proceduralGeometryConstants.BottomLevelASToLocalSpaceMatrix);

    float thit = 0.0f;
    ProceduralAttributes attrs = (ProceduralAttributes)0;
    if (rt_geometry_generator::IsIntersectionTestPassed(localRay, rt_geometry_generator::GeometryType::SDF_MiniSpheres, sceneConstants.ElapsedTime, thit, attrs.Normal))
    {
        attrs.Normal = mul(attrs.Normal, (float3x3)proceduralGeometryConstants.LocalSpaceToBottomLevelASMatrix);
        attrs.Normal = normalize(mul((float3x3)ObjectToWorld3x4(), attrs.Normal));

        ReportHit(thit, 0, attrs);
    }
}

[shader("intersection")]
void VolumetricAABB_Intersection()
{
    const uint proceduralGeometryIndex = GeometryIndex();

    const ConstantBuffer<SceneConstants> sceneConstants = ResourceDescriptorHeap[GetRootConstant(g_SceneConstantBufferIndex)];
    const StructuredBuffer<ProceduralGeometryConstants> proceduralGeometryBuffer = ResourceDescriptorHeap[GetRootConstant(g_ProceduralGeometryBufferIndex)];
    const ProceduralGeometryConstants proceduralGeometryConstants = proceduralGeometryBuffer[proceduralGeometryIndex];

    const rt_common::Ray localRay = rt_common::TransformRay(rt_common::GetObjectRay(), proceduralGeometryConstants.BottomLevelASToLocalSpaceMatrix);

    float thit = 0.0f;
    ProceduralAttributes attrs = (ProceduralAttributes)0;
    if (rt_geometry_generator::IsIntersectionTestPassed(localRay, rt_geometry_generator::GeometryType::Metaballs, sceneConstants.ElapsedTime, thit, attrs.Normal))
    {
        attrs.Normal = mul(attrs.Normal, (float3x3)proceduralGeometryConstants.LocalSpaceToBottomLevelASMatrix);
        attrs.Normal = normalize(mul((float3x3)ObjectToWorld3x4(), attrs.Normal));

        ReportHit(thit, 0, attrs);
    }
}

[shader("intersection")]
void SignedDistanceAABB_Intersection()
{
    const uint proceduralGeometryIndex = GeometryIndex();

    const ConstantBuffer<SceneConstants> sceneConstants = ResourceDescriptorHeap[GetRootConstant(g_SceneConstantBufferIndex)];
    const StructuredBuffer<ProceduralGeometryConstants> proceduralGeometryBuffer = ResourceDescriptorHeap[GetRootConstant(g_ProceduralGeometryBufferIndex)];
    const ProceduralGeometryConstants proceduralGeometryConstants = proceduralGeometryBuffer[proceduralGeometryIndex];

    const rt_common::Ray localRay = rt_common::TransformRay(rt_common::GetObjectRay(), proceduralGeometryConstants.BottomLevelASToLocalSpaceMatrix);

    float thit = 0.0f;
    ProceduralAttributes attrs = (ProceduralAttributes)0;
    if (rt_geometry_generator::IsIntersectionTestPassed(localRay, rt_geometry_generator::GeometryType::SDF_FractalPyramid, sceneConstants.ElapsedTime, thit, attrs.Normal))
    {
        attrs.Normal = mul(attrs.Normal, (float3x3)proceduralGeometryConstants.LocalSpaceToBottomLevelASMatrix);
        attrs.Normal = normalize(mul((float3x3)ObjectToWorld3x4(), attrs.Normal));

        ReportHit(thit, 0, attrs);
    }
}

[shader("miss")]
void RadianceRay_Miss(inout RadianceRayPayload payload)
{
    payload.Color = g_BackgroundColor;
}

[shader("miss")]
void ShadowRay_Miss(inout ShadowRayPayload payload)
{
    payload.IsHitted = false;
}
