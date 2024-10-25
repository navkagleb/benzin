#pragma once

#include "joint/constant_buffer_types.hpp"
#include "joint/enum_types.hpp"
#include "joint/root_constants.hpp"
#include "joint/structured_buffer_types.hpp"

// To prevent matrix transposition in CPU side
#pragma pack_matrix(row_major)

#if !defined(RenderPassConstantsType)
    struct DummyRenderPassConstants {};
    #define RenderPassConstantsType DummyRenderPassConstants
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
ConstantBuffer<joint::FrameConstants> g_FrameConstants : register(b0, space1);
ConstantBuffer<RenderPassConstantsType> g_PassConstants : register(b0, space2);

RaytracingAccelerationStructure g_TopLevelAs : register(t0, space0);

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

#define BenzinDeclareRootResource(Type, name, rootIndex) static Type name = ResourceDescriptorHeap[GetRootConstant((uint)rootIndex)]
