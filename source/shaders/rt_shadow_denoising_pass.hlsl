#include "common.hlsli"

#include "joint/constant_buffer_types.hpp"

[numthreads(8, 8, 1)]
void CS_Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    ConstantBuffer<joint::RTShadowDenoisingPassConstants> passConstants = ResourceDescriptorHeap[GetRootConstant(joint::RTShadowDenoisingPass_PassConstantBuffer)];
    RWTexture2D<float4> noisyVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RTShadowDenoisingPass_NoisyVisibilityBuffer)];
    Texture2D<float2> motionVectors = ResourceDescriptorHeap[GetRootConstant(joint::RTShadowDenoisingPass_GBufferMotionVectorsTexture)];

    Texture2D<float4> albedo = ResourceDescriptorHeap[GetRootConstant(joint::RTShadowDenoisingPass_GBufferAlbedoTexture)];

    const float2 motionVector = motionVectors.SampleLevel(g_PointWrapSampler, dispatchThreadID.xy / passConstants.TextureDimensions, 0) * passConstants.TextureDimensions;

    uint2 previousShadowPosition = dispatchThreadID.xy + motionVector;
    if (!IsInRange(previousShadowPosition.x, 0.0f, 1280.0f) || !IsInRange(previousShadowPosition.y, 0.0f, 720.0f))
    {
        previousShadowPosition = dispatchThreadID.xy;
    }

    const float4x1 dxShit = albedo.SampleLevel(g_PointWrapSampler, dispatchThreadID.xy / passConstants.TextureDimensions, 0);

    const float dxShit0 = dxShit[0][0];
    const float dxShit1 = dxShit[1][0];
    const float dxShit2 = dxShit[2][0];
    const float dxShit3 = dxShit[3][0];

    const uint currentIndex = passConstants.CurrentTextureSlot;
    const uint previousIndex = passConstants.PreviousTextureSlot;

    const float dxShit00 = dxShit[currentIndex];
    const float dxShit11 = dxShit[previousIndex];

    const float4 currentVisibility4 = noisyVisibilityBuffer[dispatchThreadID.xy];
    const float4 previousVisibility4 = noisyVisibilityBuffer[previousShadowPosition];

    const float currentVisibility = GetFloatByIndex(currentIndex, currentVisibility4);
    const float previousVisibility = GetFloatByIndex(previousIndex, previousVisibility4);

    const float4 dxShitJoin0 = { dxShit0, dxShit1, dxShit2, dxShit3 };
    const float2 dxShitJoin1 = { dxShit00, dxShit11 };

    const float alpha = 0.3f; // Temporal fade, trading temporal stability for lag
    const float filteredVisiblity = alpha * (1.0f - previousVisibility) + (1.0f - alpha) * currentVisibility;
    // const float filteredVisiblity = 0.5f * (currentVisibility + previousVisibility);

    const float dxShitMul = dxShitJoin0.x * dxShitJoin0.y * dxShitJoin0.z * dxShitJoin0.w;
    const float dxShitSub = dxShitJoin1.x - dxShitJoin1.y;
    const float dxShitResult = dxShitMul + dxShitSub;

    // noisyVisibilityBuffer[dispatchThreadID.xy][2] = dxShitResult;
    // noisyVisibilityBuffer[dispatchThreadID.xy][2] = passConstants.IsForceCurrentVisiblity ? previousVisibility : filteredVisiblity;
}
