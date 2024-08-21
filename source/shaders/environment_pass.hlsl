#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "fullscreen_helper.hlsli"

float4 PsMain(VsFullScreenTriangleOutput input) : SV_Target
{
    TextureCube<float4> cubeMap = ResourceDescriptorHeap[GetRootConstant(joint::EnvironmentPassRc_CubeMapTexture)];

    const float4 worldPosition = mul(input.ClipPosition, g_FrameConstants.Camera.InvDirectionWorldToClip);
    const float3 direction = normalize(worldPosition.xyz);

    const float4 color = cubeMap.Sample(g_LinearWrapSampler, direction);

    return LinearToGamma(color);
}
