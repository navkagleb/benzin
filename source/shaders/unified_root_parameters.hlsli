#pragma once

#include "joint/global_resources.hpp"
#include "joint/light.hpp"

// To prevent matrix transposition in CPU side
#pragma pack_matrix(row_major)

struct DummyRenderPassConsts {};

#if !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 DummyRenderPassConsts
#endif

#if !defined(BenzinRenderPassConstsType1)
    #define BenzinRenderPassConstsType1 DummyRenderPassConsts
#endif

struct RootConstants
{
    uint4 Constants[8];

    uint GetConstant(uint index)
    {
        return Constants[index >> 2][index & 3];
    }
};

ConstantBuffer<RootConstants> g_RootConstants : register(b0, space0);
ConstantBuffer<joint::FrameConsts> g_FrameConstants : register(b0, space1);
ConstantBuffer<BenzinRenderPassConstsType0> g_PassConsts0 : register(b0, space2);
ConstantBuffer<BenzinRenderPassConstsType1> g_PassConsts1 : register(b0, space3);

StructuredBuffer<joint::Light> g_Lights : register(t0, space0);
RaytracingAccelerationStructure g_SceneTlas : register(t0, space1);

SamplerState g_PointWrapSampler : register(s0, space0);
SamplerState g_PointClampSampler : register(s0, space1);
SamplerState g_LinearWrapSampler : register(s0, space2);
SamplerState g_LinearClampSampler : register(s0, space3);
SamplerState g_Anisotropic16WrapSampler : register(s0, space4);
SamplerState g_Anisotropic16ClampSampler : register(s0, space5);

SamplerState g_MinLinearClampSampler : register(s0, space6);
SamplerState g_MaxLinearClampSampler : register(s0, space7);
SamplerState g_PointWithTransparentBlackBorderSampler : register(s0, space8);

uint GetRootConstant(uint index)
{
    return g_RootConstants.GetConstant(index);
}

#define BenzinGetRootConstant(rootIndex) g_RootConstants.GetConstant((uint)rootIndex)
#define BenzinDeclareRootResource(Type, name, rootIndex) static Type name = ResourceDescriptorHeap[GetRootConstant((uint)rootIndex)]
