#include "common.hlsl"

static const float2 g_TexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

struct VS_Input
{
    uint VertexID : SV_VertexID;
};

struct VS_Output
{
    float4 PositionH : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VS_Output VS_Main(VS_Input input)
{
    VS_Output output = (VS_Output)0;
    output.TexCoord = g_TexCoords[input.VertexID];

    // From TexCoord to PositionH
    output.PositionH = float4(output.TexCoord * 2.0f - 1.0f, 0.0f, 1.0f);
    output.PositionH.y *= -1.0f;

    return output;
}

float4 PS_Main(VS_Output input) : SV_Target
{
    return g_DiffuseMaps[NonUniformResourceIndex(0)].Sample(g_LinearWrapSampler, input.TexCoord);
}
