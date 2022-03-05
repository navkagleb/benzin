struct PerObject
{
    float4x4 WorldViewProjection;
};

ConstantBuffer<PerObject> g_PerObject : register(b0);

struct VS_Input
{
    float3 Position : Position;
    float3 Normal   : Normal;
    float3 Tangent  : Tangent;
    float2 TexCoord : TexCoord;
};

struct VS_Output
{
    float4 Position : SV_Position;
    float3 Normal   : Normal;
    float3 Tangent  : Tangent;
    float2 TexCoord : TexCoord;
};

VS_Output VS_Main(VS_Input input)
{
    VS_Output output = (VS_Output) 0;
    output.Position = mul(float4(input.Position, 1.0f), g_PerObject.WorldViewProjection);
    output.Normal   = input.Normal;
    output.Tangent  = input.Tangent;
    output.TexCoord = input.TexCoord;

    return output;
}

float4 PS_Main(VS_Output input) : SV_Target
{
    return float4(input.Normal, 1.0f) + float4(0.2f, 0.2f, 0.2f, 0.2f);
}