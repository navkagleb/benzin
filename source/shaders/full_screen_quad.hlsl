#include "common.hlsli"
#include "full_screen_helper.hlsli"

uint GetFullScreenTextureMapIndex() { return g_RootConstants.Constant0; }

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
    Texture2D fullScreenTextureMap = ResourceDescriptorHeap[GetFullScreenTextureMapIndex()];

    return fullScreenTextureMap.Sample(g_LinearWrapSampler, input.TexCoord);
}
