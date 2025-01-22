#define RenderPassConstantsType joint::MipGenerationConstants
#include "unified_root_parameters.hlsli"

#include "common.hlsli"

// Ref: https://www.3dgep.com/learning-directx-12-4/
// Ref: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli

float4 ApplyFilterType(float4 color0, float4 color1)
{
    switch (g_PassConsts0.FilterType)
    {
        case joint::MipGenerationFilterType_Min: return min(color0, color1);
        case joint::MipGenerationFilterType_Max: return max(color0, color1);
        case joint::MipGenerationFilterType_Average: return (color0 + color1) * 0.5;
    }

    return g_NaN;
}

float4 ApplyFilterType(float4 color0, float4 color1, float4 color2, float4 color3)
{
    return ApplyFilterType(
        ApplyFilterType(color0, color1),
        ApplyFilterType(color2, color3)
    );
}

float4 SampleSourceMip(Texture2D<float4> sourceMip, float2 uv)
{
    switch (g_PassConsts0.FilterType)
    {
        case joint::MipGenerationFilterType_Min:
        {
            return sourceMip.SampleLevel(g_MinLinearClampSampler, uv, 0.0);
        }
        case joint::MipGenerationFilterType_Max:
        {
            return sourceMip.SampleLevel(g_MaxLinearClampSampler, uv, 0.0);
        }
        case joint::MipGenerationFilterType_Average:
        {
            return sourceMip.SampleLevel(g_LinearClampSampler, uv, 0.0);
        }
    }

    return g_NaN;
}

float4 SampleSourceForDestinationMip0(Texture2D<float4> sourceMip, uint3 dispatchThreadId)
{
    if (!g_PassConsts0.IsSourceWidthOdd && !g_PassConsts0.IsSourceHeightOdd)
    {
        const float2 uv = (dispatchThreadId.xy + 0.5) * g_PassConsts0.InvDispatchDimensions;

        return SampleSourceMip(sourceMip, uv);
    }
    else if (g_PassConsts0.IsSourceWidthOdd && !g_PassConsts0.IsSourceHeightOdd)
    {
        const float2 uv0 = (dispatchThreadId.xy + float2(0.25, 0.5)) * g_PassConsts0.InvDispatchDimensions;
        const float2 uv1 = uv0 + float2(0.5, 0.0) * g_PassConsts0.InvDispatchDimensions;

        return ApplyFilterType(
            SampleSourceMip(sourceMip, uv0),
            SampleSourceMip(sourceMip, uv1)
        );
    }
    else if (!g_PassConsts0.IsSourceWidthOdd && g_PassConsts0.IsSourceHeightOdd)
    {
        const float2 uv0 = (dispatchThreadId.xy + float2(0.5, 0.25)) * g_PassConsts0.InvDispatchDimensions;
        const float2 uv1 = uv0 + float2(0.0, 0.5) * g_PassConsts0.InvDispatchDimensions;

        return ApplyFilterType(
            SampleSourceMip(sourceMip, uv0),
            SampleSourceMip(sourceMip, uv1)
        );
    }

    const float2 uv = (dispatchThreadId.xy + float2(0.25, 0.25)) * g_PassConsts0.InvDispatchDimensions;
    const float2 uvOffset = 0.5 * g_PassConsts0.InvDispatchDimensions;

    return ApplyFilterType(
        SampleSourceMip(sourceMip, uv),
        SampleSourceMip(sourceMip, uv + float2(uvOffset.x, 0.0)),
        SampleSourceMip(sourceMip, uv + float2(0.0, uvOffset.y)),
        SampleSourceMip(sourceMip, uv + float2(uvOffset.x, uvOffset.y))
    );
}

static const uint g_ThreadPerGroupCount = joint::ThreadCount881_X * joint::ThreadCount881_Y * joint::ThreadCount881_Z;

// LocalDataShare (LDS)
groupshared float g_GroupSharedR[g_ThreadPerGroupCount];
groupshared float g_GroupSharedG[g_ThreadPerGroupCount];
groupshared float g_GroupSharedB[g_ThreadPerGroupCount];
groupshared float g_GroupSharedA[g_ThreadPerGroupCount];

void StoreSampleForGroup(uint threadIndex, float4 sample)
{
    g_GroupSharedR[threadIndex] = sample.r;
    g_GroupSharedG[threadIndex] = sample.g;
    g_GroupSharedB[threadIndex] = sample.b;
    g_GroupSharedA[threadIndex] = sample.a;
}

float4 LoadSampleForGroup(uint threadIndex)
{
    return float4(
        g_GroupSharedR[threadIndex],
        g_GroupSharedG[threadIndex],
        g_GroupSharedB[threadIndex],
        g_GroupSharedA[threadIndex]
    );
}

// Should dispatch for 1/2 resolution of source mip
[numthreads(joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z)]
void CsMain(uint groupIndex : SV_GroupIndex, uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Texture2D<float4> sourceMip = ResourceDescriptorHeap[GetRootConstant(joint::MipGenerationRc_SourceMip)];

    RWTexture2D<float4> destinationMip0 = ResourceDescriptorHeap[GetRootConstant(joint::MipGenerationRc_DestinationMip0)];
    RWTexture2D<float4> destinationMip1 = ResourceDescriptorHeap[GetRootConstant(joint::MipGenerationRc_DestinationMip1)];
    RWTexture2D<float4> destinationMip2 = ResourceDescriptorHeap[GetRootConstant(joint::MipGenerationRc_DestinationMip2)];
    RWTexture2D<float4> destinationMip3 = ResourceDescriptorHeap[GetRootConstant(joint::MipGenerationRc_DestinationMip3)];

    if (g_PassConsts0.DestinationMipCount == 0)
    {
        return;
    }

    float4 sample0 = SampleSourceForDestinationMip0(sourceMip, dispatchThreadId);

    // DestinationMip0
    {
        destinationMip0[dispatchThreadId.xy] = sample0;

        if (g_PassConsts0.DestinationMipCount == 1)
        {
            return;
        }

        StoreSampleForGroup(groupIndex, sample0);
    }

    // 7 takes 3 bits for storage (0b0111)
    // Max thread X count = 8 (from 0 to 7). The same for Y
    // So to filter the group by 'groupIndex' need to check
    // the low three bits for X and high three bits for Y

    // The 2D representation all group indices
    // 00 01 02 03 04 05 06 07
    // 08 09 10 11 12 13 14 15
    // 16 17 18 19 20 21 22 23
    // 24 25 26 27 28 29 30 31
    // 32 33 34 35 36 37 38 39
    // 40 41 42 43 44 45 46 47
    // 48 49 50 51 52 53 54 55
    // 56 57 58 59 60 61 62 63

    // DestinationMip1
    {
        GroupMemoryBarrierWithGroupSync();

        if ((groupIndex & 0b001001) == 0)
        {
            const float4 sample1 = LoadSampleForGroup(groupIndex + 1);
            const float4 sample2 = LoadSampleForGroup(groupIndex + 8);
            const float4 sample3 = LoadSampleForGroup(groupIndex + 9);

            sample0 = ApplyFilterType(sample0, sample1, sample2, sample3);
            destinationMip1[dispatchThreadId.xy >> 1] = sample0;

            StoreSampleForGroup(groupIndex, sample0);
        }
    }

    if (g_PassConsts0.DestinationMipCount == 2)
    {
        return;
    }

    // DestinationMip2
    {
        GroupMemoryBarrierWithGroupSync();

        if ((groupIndex & 0b011011) == 0)
        {
            const float4 sample1 = LoadSampleForGroup(groupIndex + 2);
            const float4 sample2 = LoadSampleForGroup(groupIndex + 16);
            const float4 sample3 = LoadSampleForGroup(groupIndex + 18);

            sample0 = ApplyFilterType(sample0, sample1, sample2, sample3);
            destinationMip2[dispatchThreadId.xy >> 2] = sample0;

            StoreSampleForGroup(groupIndex, sample0);
        }
    }

    if (g_PassConsts0.DestinationMipCount == 3)
    {
        return;
    }

    // DestinationMip3
    {
        GroupMemoryBarrierWithGroupSync();

        if ((groupIndex & 0b111111) == 0)
        {
            const float4 sample1 = LoadSampleForGroup(groupIndex + 4);
            const float4 sample2 = LoadSampleForGroup(groupIndex + 32);
            const float4 sample3 = LoadSampleForGroup(groupIndex + 36);

            sample0 = ApplyFilterType(sample0, sample1, sample2, sample3);
            destinationMip3[dispatchThreadId.xy >> 3] = sample0;
        }
    }
}
