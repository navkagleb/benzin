#if !defined(THREAD_PER_GROUP_COUNT)
    #define THREAD_PER_GROUP_COUNT 256
#endif

#include "common.hlsli"

enum RootConstant_ : uint
{
    SettingsBufferIndex = 0,
    InputTextureIndex = 1,
    OutputTextureIndex = 2
};

static const int g_MaxBlurRadius = 5;

struct Settings
{
    int BlurRadius;
    
    // Support up to 11 blur weight
    float Weight0;
    float Weight1;
    float Weight2;
    float Weight3;
    float Weight4;
    float Weight5;
    float Weight6;
    float Weight7;
    float Weight8;
    float Weight9;
    float Weight10;
};

groupshared float4 g_Cache[THREAD_PER_GROUP_COUNT + 2 * g_MaxBlurRadius];

struct CS_Input
{
    int3 GroupThreadID : SV_GroupThreadID;
    int3 DispatchThreadID : SV_DispatchThreadID;
};

[numthreads(THREAD_PER_GROUP_COUNT, 1, 1)]
void CS_HorizontalBlur(CS_Input input)
{
    ConstantBuffer<Settings> settings = ResourceDescriptorHeap[g_RootConstants.Get(SettingsBufferIndex)];
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[g_RootConstants.Get(InputTextureIndex)];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[g_RootConstants.Get(OutputTextureIndex)];

    const float weights[11] =
    { 
        settings.Weight0,
        settings.Weight1,
        settings.Weight2,
        settings.Weight3,
        settings.Weight4,
        settings.Weight5,
        settings.Weight6,
        settings.Weight7,
        settings.Weight8,
        settings.Weight9,
        settings.Weight10
    };
    
    float inputWidth;
    float inputHeight;
    inputTexture.GetDimensions(inputWidth, inputHeight);
    
    if (input.GroupThreadID.x < settings.BlurRadius)
    {
        const int inputX = max(input.DispatchThreadID.x - settings.BlurRadius, 0);
        const int inputY = input.DispatchThreadID.y;
        
        g_Cache[input.GroupThreadID.x] = inputTexture[int2(inputX, inputY)];
    }
    
    if (input.GroupThreadID.x >= THREAD_PER_GROUP_COUNT - settings.BlurRadius)
    {
        const int inputX = min(input.DispatchThreadID.x + settings.BlurRadius, inputWidth - 1);
        const int inputY = input.DispatchThreadID.y;
        
        g_Cache[input.GroupThreadID.x + 2 * settings.BlurRadius] = inputTexture[int2(inputX, inputY)];
    }

    g_Cache[input.GroupThreadID.x + settings.BlurRadius] = inputTexture[min(input.DispatchThreadID.xy, int2(inputWidth, inputHeight) - 1)];

    GroupMemoryBarrierWithGroupSync();

    float4 blurColor = float4(0, 0, 0, 0);

    for (int i = -settings.BlurRadius; i < settings.BlurRadius + 1; ++i)
    {
        const int weightIndex = i + settings.BlurRadius;
        const int cacheIndex = input.GroupThreadID.x + settings.BlurRadius + i;

        blurColor += weights[weightIndex] * g_Cache[cacheIndex];
    }

    outputTexture[input.DispatchThreadID.xy] = blurColor;
}

[numthreads(1, THREAD_PER_GROUP_COUNT, 1)]
void CS_VerticalBlur(CS_Input input)
{
    ConstantBuffer<Settings> settings = ResourceDescriptorHeap[g_RootConstants.Get(SettingsBufferIndex)];
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[g_RootConstants.Get(InputTextureIndex)];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[g_RootConstants.Get(OutputTextureIndex)];

    const float weights[11] =
    {
        settings.Weight0,
        settings.Weight1,
        settings.Weight2,
        settings.Weight3,
        settings.Weight4,
        settings.Weight5,
        settings.Weight6,
        settings.Weight7,
        settings.Weight8,
        settings.Weight9,
        settings.Weight10
    };

    float inputWidth;
    float inputHeight;
    inputTexture.GetDimensions(inputWidth, inputHeight);
    
    if (input.GroupThreadID.y < settings.BlurRadius)
    {
        const int inputX = input.DispatchThreadID.x;
        const int inputY = max(input.DispatchThreadID.y - settings.BlurRadius, 0);
        
        g_Cache[input.GroupThreadID.y] = inputTexture[int2(inputX, inputY)];
    }
    
    if (input.GroupThreadID.y >= THREAD_PER_GROUP_COUNT - settings.BlurRadius)
    {
        const int inputX = input.DispatchThreadID.x;
        const int inputY = min(input.DispatchThreadID.y + settings.BlurRadius, inputHeight - 1);
        
        g_Cache[input.GroupThreadID.y + 2 * settings.BlurRadius] = inputTexture[int2(inputX, inputY)];
    }

    g_Cache[input.GroupThreadID.y + settings.BlurRadius] = inputTexture[min(input.DispatchThreadID.xy, int2(inputWidth, inputHeight) - 1)];

    GroupMemoryBarrierWithGroupSync();

    float4 blurColor = float4(0, 0, 0, 0);

    for (int i = -settings.BlurRadius; i < settings.BlurRadius + 1; ++i)
    {
        const int weightIndex = i + settings.BlurRadius;
        const int cacheIndex = input.GroupThreadID.y + settings.BlurRadius + i;

        blurColor += weights[weightIndex] * g_Cache[cacheIndex];
    }

    outputTexture[input.DispatchThreadID.xy] = blurColor;
}
