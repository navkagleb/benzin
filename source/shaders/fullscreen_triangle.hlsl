#include "fullscreen_helper.hlsli"

VsFullScreenTriangleOutput CreateOutput(uint vertexIndex, float4 clipPosition)
{
    VsFullScreenTriangleOutput output = (VsFullScreenTriangleOutput)0;
    output.SvPosition = clipPosition;
    output.ClipPosition = clipPosition;
    output.Uv = GetFullScreenTriangleUv(vertexIndex);

    return output;
}

VsFullScreenTriangleOutput VsMain(uint vertexIndex : SV_VertexID)
{
    return CreateOutput(vertexIndex, GetFullScreenTriangleClipPosition(vertexIndex));
}

VsFullScreenTriangleOutput VsMainDepth0(uint vertexIndex : SV_VertexID)
{
    float4 clipPositionDepth0 = GetFullScreenTriangleClipPosition(vertexIndex);
    clipPositionDepth0.z = 0.0;

    return CreateOutput(vertexIndex, clipPositionDepth0);
}
