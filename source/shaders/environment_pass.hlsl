#include "joint/environment_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "color_convertions.hlsli"

BenzinDeclareRootResource(TextureCube<float4>, g_CubeMap, joint::EnvironmentResources::CubeMap);

struct VsOutput
{
    float4 m_SvPosition : SV_Position;
    float4 m_ClipPosition : ClipPosition;
};

VsOutput VsMain(uint vertexIndex : SV_VertexID)
{
    VsOutput output = (VsOutput)0;
    output.m_SvPosition = GetFullScreenTriangleClipPosition(vertexIndex);
    output.m_SvPosition.z = 0.0;
    output.m_ClipPosition = output.m_SvPosition;

    return output;
}

float4 PsMain(VsOutput input) : SV_Target
{
    const float4 worldPosition = mul(input.m_ClipPosition, GetCameraConsts().ClipToWorldNoTranslation);
    const float3 direction = normalize(worldPosition.xyz);

    const float3 linearRgb = SrgbToLinear(g_CubeMap.Sample(g_LinearWrapSampler, direction).rgb);
    return float4(linearRgb, 1.0);
}
