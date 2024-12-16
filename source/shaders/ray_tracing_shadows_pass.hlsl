#define RenderPassConstantsType joint::RayTracingShadowsConstants
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "rt_common.hlsli"
#include "sigma_denoiser/sigma_public.hlsli"
#include "space_convertions.hlsli"

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

float GetPseudoRandomFloat(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233)) / g_FrameConstants.RandomFloats01.w) * 43758.5453); // TODO
}

float3 GetLightConeSample(float3 toLightDirection, float coneAngleInRandians)
{
    float cosAngle = cos(coneAngleInRandians);

    // Generate points on the spherical cap around the north pole [1].
    // [1] See https://math.stackexchange.com/a/205589/81266
    float z = GetPseudoRandomFloat(GetRayUv()) * (1.0f - cosAngle) + cosAngle;
    float phi = GetPseudoRandomFloat(GetRayUv()) * g_TwoPi;

    float x = sqrt(1.0 - z * z) * cos(phi);
    float y = sqrt(1.0 - z * z) * sin(phi);
    float3 north = float3(0.0, 0.0, 1.0);

    // Find the rotation axis `u` and rotation angle `rot` [1]
    float3 axis = normalize(cross(north, normalize(toLightDirection)));
    float angle = acos(dot(normalize(toLightDirection), north));

    // Convert rotation axis and angle to 3x3 rotation matrix [2]
    float3x3 R = AngleAxis3x3(angle, axis);

    return mul(R, float3(x, y, z));
}

void TraceSunShadowRay(
    float3 worldPosition,
    float3 worldNormal,
    out float outDistanceToOccluder
)
{
    const float coneAngleInRadians = g_PassConstants.SunAngularRadiusInRadians * 2.0;

    RayDesc rayDesc;
    rayDesc.Origin = OffsetRayPosition(worldPosition, worldNormal);
    rayDesc.Direction = GetLightConeSample(g_PassConstants.SunDirection, coneAngleInRadians);
    rayDesc.TMin = 0.01;
    rayDesc.TMax = sigma::g_Fp16Max;

    uint rayFlags = RAY_FLAG_NONE;
    // rayFlags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
    // rayFlags |= RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    rayFlags |= RAY_FLAG_FORCE_OPAQUE; // Skip any hit shaders
    // rayFlags |= RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;

    joint::ShadowRayPayload payload;
    payload.THit = 0.0;

    TraceRay(
        g_TopLevelAs,
        rayFlags,
        g_InstanceMask,
        g_HitGroupIndex,
        g_HitGroupStride,
        g_MissShaderIndex,
        rayDesc,
        payload
    );

    outDistanceToOccluder = payload.THit;
}

BenzinDeclareRootResource(Texture2D<float4>, g_WorldNormalTex, joint::RayTracingShadowsRc_WorldNormalTex);
BenzinDeclareRootResource(Texture2D<float>, g_DepthTex, joint::RayTracingShadowsRc_DepthTex);
BenzinDeclareRootResource(RWTexture2D<float>, g_OutNoisyPenumbraTex, joint::RayTracingShadowsRc_OutNoisyPenumbraTex);

[shader("raygeneration")]
void RayGen()
{
    if (!g_FrameConstants.IsShadowsEnabled)
    {
        g_OutNoisyPenumbraTex[DispatchRaysIndex().xy] = sigma::PackPenumbra(sigma::g_Fp16Max, g_PassConstants.TanSunAngularRadius);
        return;
    }

    const float3 worldNormal = g_WorldNormalTex[DispatchRaysIndex().xy].xyz;
    const float depth = g_DepthTex[DispatchRaysIndex().xy];

    const float2 uv = GetRayUv();
    const joint::CameraConstants cameraConstants = g_FrameConstants.Camera;
    const float3 worldPosition = ReconstructWorldPosition(uv, depth, cameraConstants.ClipToView, cameraConstants.ViewToWorld);

    float distanceToOccluder;
    TraceSunShadowRay(worldPosition, worldNormal, distanceToOccluder);

    // float distanceToOccluder:
    // - distance to occluder, must follow the rules:
    //     - NoL <= 0         - 0 ( it's very important )
    //     - NoL > 0 ( hit )  - hit distance
    //     - NoL > 0 ( miss ) - >= NRD_FP16_MAX
    const float packedPenumbra = sigma::PackPenumbra(distanceToOccluder, g_PassConstants.TanSunAngularRadius);
    g_OutNoisyPenumbraTex[DispatchRaysIndex().xy] = packedPenumbra;
}

[shader("closesthit")]
void ClosestHitShader(inout joint::ShadowRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.THit = RayTCurrent();
}

[shader("miss")]
void Miss(inout joint::ShadowRayPayload payload)
{
    payload.THit = sigma::g_Fp16Max;
}
