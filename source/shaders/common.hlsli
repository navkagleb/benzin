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

SamplerState g_PointWrapSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);
SamplerState g_LinearWrapSampler : register(s2);
SamplerState g_LinearClampSampler : register(s3);
SamplerState g_Anisotropic16WrapSampler : register(s4);
SamplerState g_Anisotropic16ClampSampler : register(s5);
SamplerComparisonState g_ShadowSampler : register(s6);

static const uint g_InvalidIndex = -1;
    
static const float g_PI = 3.14159265359f;
static const float g_2PI = 2.0f * g_PI;
static const float g_InvPI = 1.0f / g_PI;
static const float g_Inv2PI = 1.0f / g_2PI;

static const float g_FloatEpsilon = 0.00001f;
static const float g_FloatInfinity = 1.#INF;

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

bool IsInRange(float val, float min, float max)
{
    return val >= min && val <= max;
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

joint::CameraConstants FetchCameraConstants()
{
    ConstantBuffer<joint::CameraConstants> cameraConstants = ResourceDescriptorHeap[GetRootConstant(joint::GlobalRC_CameraConstantBuffer)];
    return cameraConstants;
}
