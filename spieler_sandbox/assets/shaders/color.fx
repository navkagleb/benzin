struct PerObject
{
    float4x4 World;
};

struct PerPass
{
    float4x4 View;
    float4x4 Projection;
    float3 W_CameraPosition;
    float4 AmbientLight;
};

ConstantBuffer<PerPass> g_PerPass : register(b0);
ConstantBuffer<PerObject> g_PerObject : register(b1);

struct VS_Input
{
    float3 Position : Position;
    float4 Color : Color;
};

struct VS_Output
{
    float4 Position : SV_Position;
    float4 Color : Color;
};

VS_Output VS_Main(VS_Input input)
{
    float4 position = mul(float4(input.Position, 1.0f), g_PerObject.World);
    position = mul(position, g_PerPass.View);
    position = mul(position, g_PerPass.Projection);

    VS_Output output = (VS_Output) 0;
    output.Position = position;
    output.Color = input.Color;

    return output;
}

float4 PS_Main(VS_Output input) : SV_Target
{
    return input.Color;
}