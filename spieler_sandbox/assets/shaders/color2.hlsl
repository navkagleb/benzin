struct PerPass
{
    float4x4 View;
    float4x4 Projection;
    float3 W_CameraPosition;
    float4 AmbientLight;
};

struct PerObject
{
    float4x4 World;
    float4 Color;
};

ConstantBuffer<PerPass> g_Pass : register(b0);
ConstantBuffer<PerObject> g_Object : register(b1);

struct VS_Input
{
    float3 PositionL : Position;
    float3 NormalL : Normal;
    float3 Tangent : Tangent;
    float2 TexCoord : TexCoord;
};

struct VS_Output
{
    float4 PositionH : SV_Position;
    float3 PositionW : Position;
    float3 NormalW : Normal;
    float2 TexCoord : TexCoord;
};

VS_Output VS_Main(VS_Input input)
{
    VS_Output output = (VS_Output) 0;
    output.PositionW = mul(float4(input.PositionL, 1.0f), g_Object.World).xyz;
    output.NormalW = mul(input.NormalL, (float3x3) g_Object.World);

    float4 position = float4(output.PositionW, 1.0f);
    position = mul(position, g_Pass.View);
    position = mul(position, g_Pass.Projection);

    output.PositionH = position;
    output.TexCoord = input.TexCoord;

    return output;
}

float4 PS_Main(VS_Output input) : SV_Target
{
    return g_Object.Color;
}