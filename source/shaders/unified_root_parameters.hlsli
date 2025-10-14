#pragma once

#include "joint/global_resources.hpp"
#include "joint/gpu_print_resources.hpp"
#include "joint/light.hpp"

// To prevent matrix transposition in CPU side
#pragma pack_matrix(row_major)

// NOTE: Always use mul() instead of operator * for matrix-matrix and matrix-vector multiplication in HLSL
// It's the only way to ensure correct layout-aware behavior, especially when using #pragma pack_matrix

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
ConstantBuffer<joint::FrameConsts> g_FrameConsts : register(b0, space1);
ConstantBuffer<BenzinRenderPassConstsType0> g_PassConsts0 : register(b0, space2);
ConstantBuffer<BenzinRenderPassConstsType1> g_PassConsts1 : register(b0, space3);
ConstantBuffer<joint::GpuPrintConsts> g_GpuPrintConsts : register(b0, space4);

StructuredBuffer<joint::Light> g_Lights : register(t0, space0);
RaytracingAccelerationStructure g_SceneTlas : register(t0, space1);

RWByteAddressBuffer g_Stats : register(u0, space0);

SamplerState g_PointWrapSampler : register(s0, space0);
SamplerState g_PointClampSampler : register(s0, space1);
SamplerState g_LinearWrapSampler : register(s0, space2);
SamplerState g_LinearClampSampler : register(s0, space3);
SamplerState g_Anisotropic16WrapSampler : register(s0, space4);
SamplerState g_Anisotropic16ClampSampler : register(s0, space5);

SamplerState g_MinLinearClampSampler : register(s0, space6);
SamplerState g_MaxLinearClampSampler : register(s0, space7);
SamplerState g_PointWithTransparentBlackBorderSampler : register(s0, space8);

const joint::CameraConsts GetCameraConsts()
{
    return g_FrameConsts.Camera;
}

const joint::CameraConsts GetPrevCameraConsts()
{
    return g_FrameConsts.PrevCamera;
}

void InterlockedAddToStat(joint::ReadbackStat stat, uint value)
{
    g_Stats.InterlockedAdd((uint)stat * 4, value);
}

#define BenzinGetRootConstant(rootIndex) g_RootConstants.GetConstant((uint)rootIndex)
#define BenzinDeclareRootResource(Type, name, rootIndex) static Type name = ResourceDescriptorHeap[BenzinGetRootConstant(rootIndex)]
