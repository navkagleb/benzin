static const float2 g_TexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

namespace full_screen_helper
{

    float2 GetTexCoord(uint vertexID)
    {
        return g_TexCoords[vertexID];
    }

    float4 GetHomogeneousPosition(float2 texCoord)
    {
        float4 homogeneousPosition = float4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);
        homogeneousPosition.y *= -1.0f;

        return homogeneousPosition;
    }

} // namespace full_screen_helper
