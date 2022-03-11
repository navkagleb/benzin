struct PerObject
{
    float4x4 WorldViewProjection;
};

ConstantBuffer<PerObject> g_PerObject : register(b0);

struct VS_Input
{
    float4 Position : Position;
    float4 Color    : Color;
};

struct VS_Output
{
    float4 Position : SV_Position;
    float4 Color    : Color;
};

VS_Output VS_Main(VS_Input input)
{
    input.Position.w = 1.0f;

    VS_Output output    = (VS_Output) 0;
    output.Position     = mul(input.Position, g_PerObject.WorldViewProjection);
    output.Color        = input.Color;

    return output;
}

float4 PS_Main(VS_Output input) : SV_Target
{
    return input.Color;
}