#pragma once

// To prevent matrix transposition in CPU side
#pragma pack_matrix(row_major)

namespace common
{

    struct RootConstants
    {
        uint4 Constants[8];

        uint Get(uint index)
        {
            return Constants[index >> 2][index & 3];
        }
    };

    ConstantBuffer<RootConstants> g_RootConstants : register(b0, space0);

    SamplerState g_PointWrapSampler : register(s0);
    SamplerState g_PointClampSampler : register(s1);
    SamplerState g_LinearWrapSampler : register(s2);
    SamplerState g_LinearClampSampler : register(s3);
    SamplerState g_Anisotropic16WrapSampler : register(s4);
    SamplerState g_Anisotropic16ClampSampler : register(s5);
    SamplerComparisonState g_ShadowSampler : register(s6);

    static const float g_PI = 3.141592653;
    static const float g_2PI = 2 * g_PI;
    static const float g_InvPI = 1.0f / g_PI;
    static const float g_Inv2PI = 1.0f / g_2PI;

} // namespace common

#define BENZIN_GET_ROOT_CONSTANT(index) ::common::g_RootConstants.Get(index)