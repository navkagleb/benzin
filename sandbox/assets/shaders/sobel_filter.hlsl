struct CS_Input
{
    int3 DispatchThreadID : SV_DispatchThreadID;
};

// Approximates luminance ("brightness") from an RGB value. These weights are
// derived from experiment based on eye sensitivity to different wavelengths
// of light.
float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

Texture2D g_Input : register(t0);
RWTexture2D<float4> g_Output : register(u0);

[numthreads(16, 16, 1)]
void CS_Main(CS_Input input)
{
    float4 c[3][3];
    
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            int2 xy = input.DispatchThreadID.xy + int2(-1 + j, -1 + i);
            c[i][j] = g_Input[xy];
        }
    }
    
    // For each color channel, estimate partial x derivative using Sobel scheme.
    const float4 Gx = -1.0f * c[0][0] - 2.0f * c[1][0] - 1.0f * c[2][0] + 1.0f * c[0][2] + 2.0f * c[1][2] + 1.0f * c[2][2];
    
    // For each color channel, estimate partial y derivative using Sobel scheme.
    const float4 Gy = -1.0f * c[2][0] - 2.0f * c[2][1] - 1.0f * c[2][1] + 1.0f * c[0][0] + 2.0f * c[0][1] + 1.0f * c[0][2];
    
    // Gradient is (Gx, Gy). For each color channel, compute magnitude to
    // get maximum rate of change.
    float4 magnitude = sqrt(Gx * Gx + Gy * Gy);
    
    // Make edges black, and nonedges white.
    magnitude = 1.0f - saturate(CalcLuminance(magnitude.rgb));
    
    g_Output[input.DispatchThreadID.xy] = magnitude;
}
