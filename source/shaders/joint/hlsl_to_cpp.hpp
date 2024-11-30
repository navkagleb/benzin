#pragma once

#define BenzinAlign16

#ifdef __cplusplus

    #undef BenzinAlign16
    #define BenzinAlign16 __declspec(align(16))

    #define JointDefineEnum(EnumName, ...) \
        enum class EnumName : uint \
        { \
            __VA_ARGS__ \
        }; \
        BenzinEnableUnaryPlusForEnum(EnumName)

namespace joint
{

    using uint = uint32_t;
    using uint2 = DirectX::XMUINT2;
    using uint3 = DirectX::XMUINT3;
    using uint4 = DirectX::XMUINT4;

    using float2 = DirectX::XMFLOAT2;
    using float3 = DirectX::XMFLOAT3;
    using float4 = DirectX::XMFLOAT4;
    using float4x4 = DirectX::XMMATRIX;

    static_assert(sizeof(uint2) == sizeof(uint) * 2);
    static_assert(sizeof(uint3) == sizeof(uint) * 3);
    static_assert(sizeof(uint4) == sizeof(uint) * 4);

    static_assert(sizeof(float2) == sizeof(float) * 2);
    static_assert(sizeof(float3) == sizeof(float) * 3);
    static_assert(sizeof(float4) == sizeof(float) * 4);
    static_assert(sizeof(float4x4) == sizeof(float) * 4 * 4);
}

#else

    #undef BenzinEnableUnaryPlusForEnum
    #define BenzinEnableUnaryPlusForEnum(EnumT)

    #define JointDefineEnum(EnumName, ...) namespace EnumName { enum struct Enum : int { __VA_ARGS__ }; }

#endif
