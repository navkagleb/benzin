#if !defined(THREAD_PER_GROUP_COUNT)
    #define THREAD_PER_GROUP_COUNT 256
#endif

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

ConstantBuffer<Settings> g_Settings : register(b0);

Texture2D g_Input : register(t0);
RWTexture2D<float4> g_Output : register(u0);

groupshared float4 g_Cache[THREAD_PER_GROUP_COUNT + 2 * g_MaxBlurRadius];

struct CS_Input
{
    int3 GroupThreadID : SV_GroupThreadID;
    int3 DispatchThreadID : SV_DispatchThreadID;
};

[numthreads(THREAD_PER_GROUP_COUNT, 1, 1)]
void CS_HorizontalBlur(CS_Input input)
{
    const float weights[11] =
    { 
        g_Settings.Weight0,
        g_Settings.Weight1,
        g_Settings.Weight2,
        g_Settings.Weight3,
        g_Settings.Weight4,
        g_Settings.Weight5,
        g_Settings.Weight6,
        g_Settings.Weight7,
        g_Settings.Weight8,
        g_Settings.Weight9,
        g_Settings.Weight10
    };
    
    float inputWidth;
    float inputHeight;
    g_Input.GetDimensions(inputWidth, inputHeight);
    
    if (input.GroupThreadID.x < g_Settings.BlurRadius)
    {
        const int inputX = max(input.DispatchThreadID.x - g_Settings.BlurRadius, 0);
        const int inputY = input.DispatchThreadID.y;
        
        g_Cache[input.GroupThreadID.x] = g_Input[int2(inputX, inputY)];
    }
    
    if (input.GroupThreadID.x >= THREAD_PER_GROUP_COUNT - g_Settings.BlurRadius)
    {
        const int inputX = min(input.DispatchThreadID.x + g_Settings.BlurRadius, inputWidth - 1);
        const int inputY = input.DispatchThreadID.y;
        
        g_Cache[input.GroupThreadID.x + 2 * g_Settings.BlurRadius] = g_Input[int2(inputX, inputY)];
    }

    g_Cache[input.GroupThreadID.x + g_Settings.BlurRadius] = g_Input[min(input.DispatchThreadID.xy, int2(inputWidth, inputHeight) - 1)];

    GroupMemoryBarrierWithGroupSync();

    float4 blurColor = float4(0, 0, 0, 0);

    for (int i = -g_Settings.BlurRadius; i < g_Settings.BlurRadius + 1; ++i)
    {
        const int weightIndex = i + g_Settings.BlurRadius;
        const int cacheIndex = input.GroupThreadID.x + g_Settings.BlurRadius + i;

        blurColor += weights[weightIndex] * g_Cache[cacheIndex];
    }

    g_Output[input.DispatchThreadID.xy] = blurColor;
}

[numthreads(1, THREAD_PER_GROUP_COUNT, 1)]
void CS_VerticalBlur(CS_Input input)
{
    const float weights[11] =
    {
        g_Settings.Weight0,
        g_Settings.Weight1,
        g_Settings.Weight2,
        g_Settings.Weight3,
        g_Settings.Weight4,
        g_Settings.Weight5,
        g_Settings.Weight6,
        g_Settings.Weight7,
        g_Settings.Weight8,
        g_Settings.Weight9,
        g_Settings.Weight10
    };

    float inputWidth;
    float inputHeight;
    g_Input.GetDimensions(inputWidth, inputHeight);
    
    if (input.GroupThreadID.y < g_Settings.BlurRadius)
    {
        const int inputX = input.DispatchThreadID.x;
        const int inputY = max(input.DispatchThreadID.y - g_Settings.BlurRadius, 0);
        
        g_Cache[input.GroupThreadID.y] = g_Input[int2(inputX, inputY)];
    }
    
    if (input.GroupThreadID.y >= THREAD_PER_GROUP_COUNT - g_Settings.BlurRadius)
    {
        const int inputX = input.DispatchThreadID.x;
        const int inputY = min(input.DispatchThreadID.y + g_Settings.BlurRadius, inputHeight - 1);
        
        g_Cache[input.GroupThreadID.y + 2 * g_Settings.BlurRadius] = g_Input[int2(inputX, inputY)];
    }

    g_Cache[input.GroupThreadID.y + g_Settings.BlurRadius] = g_Input[min(input.DispatchThreadID.xy, int2(inputWidth, inputHeight) - 1)];

    GroupMemoryBarrierWithGroupSync();

    float4 blurColor = float4(0, 0, 0, 0);

    for (int i = -g_Settings.BlurRadius; i < g_Settings.BlurRadius + 1; ++i)
    {
        const int weightIndex = i + g_Settings.BlurRadius;
        const int cacheIndex = input.GroupThreadID.y + g_Settings.BlurRadius + i;

        blurColor += weights[weightIndex] * g_Cache[cacheIndex];
    }

    g_Output[input.DispatchThreadID.xy] = blurColor;
}