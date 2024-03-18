#include "common.hlsli"
#include "rt_common.hlsli"

#include "gbuffer.hlsli"
#include "random.hlsli"

float3x3 AngleAxis3x3(float angle, float3 axis)
{
    // Ref: https://gist.github.com/keijiro/ee439d5e7388f3aafc5296005c8c3f33
    // Rotation with angle (in radians) and axis

    float c, s;
    sincos(angle, s, c);

    const float t = 1.0f - c;
    const float x = axis.x;
    const float y = axis.y;
    const float z = axis.z;

    return float3x3(
        t * x * x + c,      t * x * y - s * z,  t * x * z + s * y,
        t * x * y + s * z,  t * y * y + c,      t * y * z - s * x,
        t * x * z - s * y,  t * y * z + s * x,  t * z * z + c
    );
}

static const float g_RayOrigin = 1.0f / 32.0f;
static const float g_FloatScale = 1.0f / 65536.0f;
static const float g_IntScale = 256.0f;

float3 OffsetRayPosition(float3 position, float3 n)
{
    // Normal points outward for rays exiting the surface, else is flipped

    const int3 intOffset = g_IntScale * n;

    const float3 intPosition = float3(
        asfloat(asint(position.x) + ((position.x < 0.0f) ? -intOffset.x : intOffset.x)),
        asfloat(asint(position.y) + ((position.y < 0.0f) ? -intOffset.y : intOffset.y)),
        asfloat(asint(position.z) + ((position.z < 0.0f) ? -intOffset.z : intOffset.z))
    );
    
    return float3(
        abs(position.x) < g_RayOrigin ? position.x + g_FloatScale * n.x : intPosition.x,
        abs(position.y) < g_RayOrigin ? position.y + g_FloatScale * n.y : intPosition.y,
        abs(position.z) < g_RayOrigin ? position.z + g_FloatScale * n.z : intPosition.z
    );

}

static const uint g_InstanceMask = ~0;
static const uint g_HitGroupIndex = 0;
static const uint g_HitGroupStride = 1;
static const uint g_MissShaderIndex = 0;

static const float3 g_UpDirection = float3(0.0, 1.0f, 0.0f);

float GetLightConeAngle(float3 worldPosition, joint::PointLight pointLight)
{
    const float3 toLightDirection = normalize(pointLight.WorldPosition - worldPosition);

    float3 perpendicularToLightDirection = cross(toLightDirection, g_UpDirection);
    if (all(perpendicularToLightDirection == 0.0f))
    {
        perpendicularToLightDirection.x = 1.0f;
    }

    const float3 lightEdgePoint = pointLight.WorldPosition + perpendicularToLightDirection * pointLight.GeometryRadius;
    const float3 toLightEdgeDirection = normalize(lightEdgePoint - worldPosition);

    const float coneAngle = acos(dot(toLightDirection, toLightEdgeDirection)) * 2.0f;
    return coneAngle;
}

float3 GetLightConeSample(float3 toLightDirection, float coneAngle, float2 uvSeed)
{
    uint seed = 123;

    float cosAngle = cos(coneAngle);

    // Generate points on the spherical cap around the north pole [1].
    // [1] See https://math.stackexchange.com/a/205589/81266
    float z = GetRandomFloatUV(uvSeed) * (1.0f - cosAngle) + cosAngle;
    float phi = GetRandomFloatUV(uvSeed) * g_2PI;

    float x = sqrt(1.0f - z * z) * cos(phi);
    float y = sqrt(1.0f - z * z) * sin(phi);
    float3 north = float3(0.f, 0.f, 1.f);

    // Find the rotation axis `u` and rotation angle `rot` [1]
    float3 axis = normalize(cross(north, normalize(toLightDirection)));
    float angle = acos(dot(normalize(toLightDirection), north));

    // Convert rotation axis and angle to 3x3 rotation matrix [2]
    float3x3 R = AngleAxis3x3(angle, axis);

    return mul(R, float3(x, y, z));
}

bool TraceShadowRay(float3 worldPosition, float3 worldNormal, joint::PointLight pointLight, float2 uvSeed)
{
    float3 toLightDirection = pointLight.WorldPosition - worldPosition;
    float distanceToLight = length(toLightDirection);
    toLightDirection /= distanceToLight;

    const float coneAngle = GetLightConeAngle(worldPosition, pointLight);
    const float3 coneDirection = GetLightConeSample(toLightDirection, coneAngle, uvSeed);

    RayDesc rayDesc;
    rayDesc.Origin = OffsetRayPosition(worldPosition, worldNormal);
    rayDesc.Direction = coneDirection;
    rayDesc.TMin = 0.01f;
    rayDesc.TMax = distanceToLight;

    const uint rayFlags =
        // RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
        RAY_FLAG_FORCE_OPAQUE | // Skip any hit shaders
        RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;

    joint::ShadowRayPayload payload;
    payload.IsHitted = true;

    TraceRay(
        g_TopLevelAS,
        rayFlags,
        g_InstanceMask,
        g_HitGroupIndex,
        g_HitGroupStride,
        g_MissShaderIndex,
        rayDesc,
        payload
    );

    return payload.IsHitted;
}

[shader("raygeneration")]
void RayGen()
{
    const joint::FrameConstants frameConstants = FetchFrameConstants();
    const joint::CameraConstants cameraConstants = FetchCurrentCameraConstants();
    const joint::RtShadowPassConstants passConstants = FetchConstantBuffer<joint::RtShadowPassConstants>(joint::RtShadowPassRc_PassConstantBuffer);

    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowPassRc_GBufferWorldNormalTexture)];
    Texture2D<float> depthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowPassRc_GBufferDepthTexture)];

    StructuredBuffer<joint::PointLight> pointLightBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowPassRc_PointLightBuffer)];

    RWTexture2D<float4> visibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowPassRc_VisiblityBuffer)];

    const float2 uv = GetRayUv();
    const float3 worldNormal = worldNormalTexture.SampleLevel(g_PointWrapSampler, uv, 0).xyz;
    const float depth = depthBuffer.SampleLevel(g_PointWrapSampler, uv, 0);

    const joint::PointLight pointLight = pointLightBuffer[0];

    const float3 worldPosition = ReconstructWorldPositionFromDepth(uv, depth, cameraConstants.InverseProjection, cameraConstants.InverseView).xyz;

    uint hittedSum = 0;
    for (uint i = 0; i < passConstants.RaysPerPixel; ++i)
    {
        const float2 uvSeed = (uv + i * frameConstants.DeltaTime) * frameConstants.DeltaTime;
        hittedSum += TraceShadowRay(worldPosition, worldNormal, pointLight, uvSeed);
    }

    visibilityBuffer[DispatchRaysIndex().xy][passConstants.CurrentTextureSlot] = (float)hittedSum / passConstants.RaysPerPixel;
}

[shader("miss")]
void Miss(inout joint::ShadowRayPayload payload)
{
    payload.IsHitted = false;
}
