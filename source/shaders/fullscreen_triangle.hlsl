#include "common.hlsli"
#include "fullscreen_helper.hlsli"

fullscreen_helper::VS_Output VS_Main(uint32_t vertexID : SV_VertexID)
{
    fullscreen_helper::VS_Output output = (fullscreen_helper::VS_Output)0;
    output.HomogeneousPosition = fullscreen_helper::GetTriangleHomogeneousPosition(vertexID);
    output.ClipPosition = output.HomogeneousPosition;
    output.UV = fullscreen_helper::GetTriangleUV(vertexID);

    return output;
}

fullscreen_helper::VS_Output VS_MainDepth1(uint32_t vertexID : SV_VertexID)
{
    float4 homogeneousPosition = fullscreen_helper::GetTriangleHomogeneousPosition(vertexID);
    homogeneousPosition.z = 1.0f;

    fullscreen_helper::VS_Output output = (fullscreen_helper::VS_Output)0;
    output.HomogeneousPosition = homogeneousPosition;
    output.ClipPosition = homogeneousPosition;
    output.UV = fullscreen_helper::GetTriangleUV(vertexID);

    return output;
}
