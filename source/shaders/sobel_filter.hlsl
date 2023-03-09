#include "common.hlsli"

uint GetInputTextureIndex() { return g_RootConstants.Constant0; }
uint GetOutputTextureIndex() { return g_RootConstants.Constant1; }

// Approximates luminance ("brightness") from an RGB value. These weights are
// derived from experiment based on eye sensitivity to different wavelengths
// of light.
float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(16, 16, 1)]
void CS_Main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D inputTexture = ResourceDescriptorHeap[GetInputTextureIndex()];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[GetOutputTextureIndex()];

    float4 c[3][3];
    
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        [unroll]
        for (int j = 0; j < 3; ++j)
        {
            const int2 xy = dispatchThreadID.xy + int2(-1 + j, -1 + i);
            c[i][j] = inputTexture[xy];
        }
    }
    
    // For each color channel, estimate partial x derivative using Sobel scheme.
    const float4 gx = -1.0f * c[0][0] - 2.0f * c[1][0] - 1.0f * c[2][0] + 1.0f * c[0][2] + 2.0f * c[1][2] + 1.0f * c[2][2];
    
    // For each color channel, estimate partial y derivative using Sobel scheme.
    const float4 gy = -1.0f * c[2][0] - 2.0f * c[2][1] - 1.0f * c[2][1] + 1.0f * c[0][0] + 2.0f * c[0][1] + 1.0f * c[0][2];
    
    // Gradient is (Gx, Gy). For each color channel, compute magnitude to
    // get maximum rate of change.
    float4 magnitude = sqrt(gx * gx + gy * gy);
    
    // Make edges black, and nonedges white.
    magnitude = 1.0f - saturate(CalcLuminance(magnitude.rgb));
    
    outputTexture[dispatchThreadID.xy] = magnitude;
}
