#include "common.hlsli"
#include "fullscreen_helper.hlsli"

float4 PS_Main(VS_FullScreenTriangleOutput input) : SV_Target
{
    const joint::CameraConstants cameraConstants = FetchCurrentCameraConstants();
    TextureCube<float4> cubeMap = ResourceDescriptorHeap[GetRootConstant(joint::EnvironmentPassRC_CubeMapTexture)];

    const float4 worldPosition = mul(input.ClipPosition, cameraConstants.InverseViewDirectionProjection);
    const float3 direction = normalize(worldPosition.xyz);

    const float4 color = cubeMap.Sample(g_LinearWrapSampler, direction);

    return LinearToGamma(color);
}
