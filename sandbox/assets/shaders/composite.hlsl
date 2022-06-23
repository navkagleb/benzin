Texture2D g_BaseMap : register(t0);
Texture2D g_EdgeMap : register(t1);

SamplerState g_SamplerPointWrap : register(s0);
SamplerState g_SamplerPointClamp : register(s1);
SamplerState g_SamplerLinearWrap : register(s2);
SamplerState g_SamplerLinearClamp : register(s3);
SamplerState g_SamplerAnisotropicWrap : register(s4);
SamplerState g_SamplerAnisotropicClamp : register(s5);

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
    float4 PosH : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VS_Output VS_Main(VS_Input input)
{
    VS_Output output = (VS_Output)0;
    output.TexCoord = g_TexCoords[input.VertexID];
    
    // Map [0,1]^2 to NDC space.
    output.PosH = float4(2.0f * output.TexCoord.x - 1.0f, 1.0f - 2.0f * output.TexCoord.y, 0.0f, 1.0f);
    
    return output;
}

float4 PS_Main(VS_Output input) : SV_Target
{
    const float4 c = g_BaseMap.Sample(g_SamplerPointClamp, input.TexCoord);
    const float4 e = g_EdgeMap.Sample(g_SamplerPointClamp, input.TexCoord);
    
    return c * e;
}
