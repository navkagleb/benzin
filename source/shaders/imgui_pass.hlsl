#include "joint/imgui_resources.hpp"
#include "unified_root_parameters.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_Texture, joint::ImGuiResources::Texture);

struct VsInput
{
    float2 Position : Position;
    float2 Uv : Uv;
    float4 Color : Color;
};

struct VsOutput
{
    float4 Position : SV_Position;
    float4 Color : Color;
    float2 Uv : Uv;
};

VsOutput VsMain(VsInput input)
{
    VsOutput output;
    output.Position = mul(float4(input.Position, 0.0, 1.0), g_PassConsts0.ViewToClipOrtho);
    output.Color = input.Color;
    output.Uv  = input.Uv;

    return output;
}

float4 PsMain(VsOutput input) : SV_Target
{
    const joint::ImGuiSamplerIndex samplerIndex = (joint::ImGuiSamplerIndex)BenzinGetRootConstant(joint::ImGuiResources::SamplerIndex);

    if (samplerIndex == joint::ImGuiSamplerIndex::Linear)
    {
        return input.Color * g_Texture.SampleLevel(g_LinearClampSampler, input.Uv, 0.0);
    }

    return input.Color * g_Texture.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
}
