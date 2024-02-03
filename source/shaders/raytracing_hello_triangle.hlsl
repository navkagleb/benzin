#include "common.hlsli"

enum RootConstant
{
    g_TopLevelASIndex,
    g_RayGenConstantBufferIndex,
    g_RaytracingOutputTextureIndex,
};

struct ViewportRect
{
    float Left;
    float Top;
    float Right;
    float Bottom;
};

struct RayGenConstants
{
    ViewportRect Viewport;
    ViewportRect Stencil;
};

struct Ray
{
    float3 Direction;
    float3 Origin;
};

struct RayPayload
{
    float4 Color;
};

bool IsInsideViewport(float2 p, ViewportRect viewport)
{
    return (p.x >= viewport.Left && p.x <= viewport.Right) && (p.y >= viewport.Top && p.y <= viewport.Bottom);
}

[shader("raygeneration")]
void RayGen()
{
    RaytracingAccelerationStructure topLevelAS = ResourceDescriptorHeap[GetRootConstant(g_TopLevelASIndex)];
    ConstantBuffer<RayGenConstants> rayGenConstants = ResourceDescriptorHeap[GetRootConstant(g_RayGenConstantBufferIndex)];
    RWTexture2D<float4> raytracingOutputTexture = ResourceDescriptorHeap[GetRootConstant(g_RaytracingOutputTextureIndex)];

    const float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

    const ViewportRect viewport = rayGenConstants.Viewport;

    // Orthographic projection since we're raytracing in screen space.
    RayDesc ray;
    ray.Direction = float3(0.0f, 0.0f, 1.0f);
    ray.Origin = float3(
        lerp(viewport.Left, viewport.Right, lerpValues.x),
        lerp(viewport.Top, viewport.Bottom, lerpValues.y),
        0.0f
    );
    ray.TMin = 0.001f;
    ray.TMax = 1000.0f;

    if (IsInsideViewport(ray.Origin.xy, rayGenConstants.Stencil))
    {
        const uint instanceInclusionMask = ~0;
        const uint contributionToHitGroupIndex = 0;
        const uint multiplierForGeometryContributionToHitGroupIndex = 1;
        const uint missShaderIndex = 0;

        RayPayload payload = { float4(0.0f, 0.0f, 0.0f, 0.0f) };

        TraceRay(
            topLevelAS,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            instanceInclusionMask,
            contributionToHitGroupIndex,
            multiplierForGeometryContributionToHitGroupIndex,
            missShaderIndex,
            ray,
            payload
        );

        // Write the raytraced color to the output texture.
        raytracingOutputTexture[DispatchRaysIndex().xy] = payload.Color;
    }
    else
    {
        // Render interpolated DispatchRaysIndex outside the stencil window
        raytracingOutputTexture[DispatchRaysIndex().xy] = float4(lerpValues, 0.0f, 1.0f);
    }
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    const float2 barycentrics = attributes.barycentrics;
    const float3 barycentricsColor = float3(1.0f - attributes.barycentrics.x - attributes.barycentrics.y, attributes.barycentrics.x, attributes.barycentrics.y);

    payload.Color = float4(barycentricsColor, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
    payload.Color = float4(0.1f, 0.1f, 0.1f, 1.0f);
}
