#include "joint/imgui_resources.hpp"
#include "unified_root_parameters.hlsli"

BenzinDeclareRenderPassConsts(joint::ImGuiConsts, g_PassConsts);
BenzinDeclareRootResource(Texture2D<float4>, g_Texture, joint::ImGuiResources::Texture);

struct VsInput
{
    float2 m_Position : Position;
    float2 m_Uv : Uv;
    float4 m_Color : Color;
};

struct VsOutput
{
    float4 m_Position : SV_Position;
    float4 m_Color : Color;
    float2 m_Uv : Uv;
};

VsOutput VsMain(VsInput input)
{
    VsOutput output = (VsOutput)0;
    output.m_Position = mul(float4(input.m_Position, 0.0, 1.0), g_PassConsts.m_ViewToClipOrtho);
    output.m_Color = input.m_Color;
    output.m_Uv = input.m_Uv;

    return output;
}

float4 PsMain(VsOutput input) : SV_Target
{
    const joint::ImGuiSamplerIndex samplerIndex = (joint::ImGuiSamplerIndex)BenzinGetRootConstant(joint::ImGuiResources::SamplerIndex);

    const float4 textureSample = samplerIndex == joint::ImGuiSamplerIndex::Linear
        ? g_Texture.Sample(g_LinearClampSampler, input.m_Uv)
        : g_Texture.Sample(g_PointClampSampler, input.m_Uv);

    return input.m_Color * textureSample;
}
