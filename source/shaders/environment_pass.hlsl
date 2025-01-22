#include "joint/environment_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "fullscreen_helper.hlsli"

BenzinDeclareRootResource(TextureCube<float4>, g_CubeMap, joint::Rc_Environment::CubeMap);

float4 PsMain(VsFullScreenTriangleOutput input) : SV_Target
{
    const float4 worldPosition = mul(input.ClipPosition, g_FrameConstants.Camera.ClipToWorldNoTranslation);
    const float3 direction = normalize(worldPosition.xyz);

    const float4 color = g_CubeMap.Sample(g_LinearWrapSampler, direction);

    return LinearToGamma(color);
}
