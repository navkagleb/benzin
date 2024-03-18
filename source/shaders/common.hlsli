#pragma once

// To prevent matrix transposition in CPU side
#pragma pack_matrix(row_major)

#include "joint/constant_buffer_types.hpp"
#include "joint/enum_types.hpp"
#include "joint/root_constants.hpp"
#include "joint/structured_buffer_types.hpp"

struct RootConstants
{
    uint4 Constants[8];

    uint Get(uint index)
    {
        return Constants[index >> 2][index & 3];
    }
};

ConstantBuffer<RootConstants> g_RootConstants : register(b0, space0);
RaytracingAccelerationStructure g_TopLevelAS : register(t0, space0);

SamplerState g_PointWrapSampler : register(s0, space0);
SamplerState g_PointClampSampler : register(s0, space1);
SamplerState g_LinearWrapSampler : register(s0, space2);
SamplerState g_LinearClampSampler : register(s0, space3);
SamplerState g_Anisotropic16WrapSampler : register(s0, space4);
SamplerState g_Anisotropic16ClampSampler : register(s0, space5);
SamplerState g_PointWithTransparentBlackBorderSampler : register(s0, space6);

static const uint g_InvalidIndex = -1;
    
static const float g_PI = 3.14159265359f;
static const float g_2PI = 2.0f * g_PI;
static const float g_InvPI = 1.0f / g_PI;
static const float g_Inv2PI = 1.0f / g_2PI;

static const float g_Epsilon = 0.0001;
static const float g_FloatInfinity = 1.#INF;
static const float g_NaN = 0.0f / 0.0f;

static const float4x4 g_IdentityMatrix =
{
    { 1.0, 0.0f, 0.0f, 0.0f },
    { 0.0, 1.0f, 0.0f, 0.0f },
    { 0.0, 0.0f, 1.0f, 0.0f },
    { 0.0, 0.0f, 0.0f, 1.0f },
};

float3 LinearToGamma(float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

float4 LinearToGamma(float4 color)
{
    return pow(color, 1.0f / 2.2f);
}

template <typename T>
bool IsInRange(T value, T min, T max)
{
    return all(value >= min) && all(value <= max);
}

void Swap(inout float lhs, inout float rhs)
{
    const float temp = lhs;
    lhs = rhs;
    rhs = lhs;
}

float DegreesToRadians(float degrees)
{
    return degrees * g_PI / 180.0f;
}

uint GetRootConstant(uint index)
{
    return g_RootConstants.Get(index);
}

ConstantBuffer<joint::FrameConstants> FetchFrameConstantBuffer()
{
    return ResourceDescriptorHeap[GetRootConstant(joint::GlobalRc_FrameConstantBuffer)];
}
        
template <typename ConstantsT>
ConstantsT FetchConstantBuffer(uint rootConstantIndex)
{
    ConstantBuffer<ConstantsT> constants = ResourceDescriptorHeap[GetRootConstant(rootConstantIndex)];
    return constants;
}

joint::FrameConstants FetchFrameConstants()
{
    return FetchConstantBuffer<joint::FrameConstants>(joint::GlobalRc_FrameConstantBuffer);
}

joint::CameraConstants FetchCurrentCameraConstants()
{
    return FetchConstantBuffer<joint::DoubleFrameCameraConstants>(joint::GlobalRc_CameraConstantBuffer).CurrentFrame;
}

ConstantBuffer<joint::DoubleFrameCameraConstants> FetchDoubleFrameCameraConstants()
{
    return ResourceDescriptorHeap[GetRootConstant(joint::GlobalRc_CameraConstantBuffer)];
}
