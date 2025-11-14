#pragma once

#include "joint/global_resources.hpp"
#include "joint/gpu_print_resources.hpp"
#include "joint/light.hpp"

// To prevent matrix transposition in CPU side
#pragma pack_matrix(row_major)

// NOTE: Always use mul() instead of operator * for matrix-matrix and matrix-vector multiplication in HLSL
// It's the only way to ensure correct layout-aware behavior, especially when using #pragma pack_matrix

struct DummyRenderPassConsts {};

#if !defined(BenzinRenderPassConstsType)
    #define BenzinRenderPassConstsType DummyRenderPassConsts
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
ConstantBuffer<BenzinRenderPassConstsType> g_PassConsts : register(b0, space2);
ConstantBuffer<joint::GpuPrintConsts> g_GpuPrintConsts : register(b0, space3);
ConstantBuffer<joint::Light> g_SunLightConsts : register(b0, space4);

RaytracingAccelerationStructure g_SceneTlas : register(t0, space0);

RWByteAddressBuffer g_Stats : register(u0, space0);

// TODO: Remove wrap samplers
SamplerState g_PointWrapSampler : register(s0, space0);
SamplerState g_PointClampSampler : register(s0, space1);
SamplerState g_LinearWrapSampler : register(s0, space2);
SamplerState g_LinearClampSampler : register(s0, space3);
SamplerState g_MinLinearClampSampler : register(s0, space4);

const joint::CameraConsts GetCameraConsts()
{
    return g_FrameConsts.m_Camera;
}

const joint::CameraConsts GetPrevCameraConsts()
{
    return g_FrameConsts.m_PrevCamera;
}

void InterlockedAddToStat(joint::ReadbackStat stat, uint value)
{
    g_Stats.InterlockedAdd((uint)stat * 4, value);
}

#define BenzinGetRootConstant(rootIndex) g_RootConstants.GetConstant((uint)rootIndex)
#define BenzinDeclareRootResource(Type, name, rootIndex) static Type name = ResourceDescriptorHeap[BenzinGetRootConstant(rootIndex)]
