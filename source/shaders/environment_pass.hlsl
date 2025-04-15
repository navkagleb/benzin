#include "joint/environment_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "color_convertions.hlsli"
#include "fullscreen_helper.hlsli"

BenzinDeclareRootResource(TextureCube<float4>, g_CubeMap, joint::EnvironmentResources::CubeMap);

float4 PsMain(VsFullScreenTriangleOutput input) : SV_Target
{
    const float4 worldPosition = mul(input.ClipPosition, GetCameraConsts().ClipToWorldNoTranslation);
    const float3 direction = normalize(worldPosition.xyz);

    const float3 linearRgb = SrgbToLinear(g_CubeMap.Sample(g_LinearWrapSampler, direction).rgb);
    return float4(linearRgb, 1.0);
}
