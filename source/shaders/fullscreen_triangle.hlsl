#include "fullscreen_helper.hlsli"

VS_FullScreenTriangleOutput CreateOutput(uint vertexIndex, float4 homogeneousPosition)
{
    VS_FullScreenTriangleOutput output = (VS_FullScreenTriangleOutput)0;
    output.HomogeneousPosition = homogeneousPosition;
    output.ClipPosition = output.HomogeneousPosition;
    output.UV = GetFullScreenTriangleUV(vertexIndex);

    return output;
}

VS_FullScreenTriangleOutput VS_Main(uint vertexIndex : SV_VertexID)
{
    return CreateOutput(vertexIndex, GetFullScreenTriangleHomogeneousPosition(vertexIndex));
}

VS_FullScreenTriangleOutput VS_MainDepth1(uint vertexIndex : SV_VertexID)
{
    return CreateOutput(vertexIndex, GetFullScreenTriangleHomogeneousPositionDepth1(vertexIndex));
}
