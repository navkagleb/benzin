#include "common.hlsli"
#include "full_screen_helper.hlsli"

uint GetBaseTextureMapIndex() { return g_RootConstants.Constant0; }
uint GetEdgeTextureMapIndex() { return g_RootConstants.Constant1; }

struct VS_Output
{
    float4 HomogeneousPosition : SV_Position;
    float2 TexCoord : TexCoord;
};

VS_Output VS_Main(uint vertexID : SV_VertexID)
{
    VS_Output output = (VS_Output)0;
    output.TexCoord = full_screen_helper::GetTexCoord(vertexID);
    output.HomogeneousPosition = full_screen_helper::GetHomogeneousPosition(output.TexCoord);

    return output;
}

float4 PS_Main(VS_Output input) : SV_Target
{
    Texture2D baseTextureMap = ResourceDescriptorHeap[GetBaseTextureMapIndex()];
    Texture2D edgeTextureMap = ResourceDescriptorHeap[GetEdgeTextureMapIndex()];

    const float4 baseSample = baseTextureMap.Sample(g_PointClampSampler, input.TexCoord);
    const float4 edgeSample = edgeTextureMap.Sample(g_PointClampSampler, input.TexCoord);

    return baseSample * edgeSample;
}
