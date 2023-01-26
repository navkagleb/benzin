#include "common.hlsl"

struct VS_INPUT
{
    float3 PositionL : Position;
    float NormalL : Normal;
    float2 TexCoord : TexCoord;
};

struct VS_OUTPUT
{
    float4 PositionH : SV_Position;
    float3 PositionL : Position;
};

VS_OUTPUT VS_Main(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    const EntityData entityData = g_Entities[instanceID];

    VS_OUTPUT output = (VS_OUTPUT)0;
    output.PositionL = input.PositionL;

    float4 positionW = mul(float4(input.PositionL, 1.0f), entityData.World);
    positionW.xyz += g_Pass.CameraPositionW;

    output.PositionH = mul(positionW, g_Pass.ViewProjection).xyww;

    return output;
}

float4 PS_Main(VS_OUTPUT input) : SV_Target
{
    return g_CubeMap.Sample(g_LinearWrapSampler, input.PositionL);
}
