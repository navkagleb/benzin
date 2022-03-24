#pragma once

#include <cmath>

#include <DirectXMath.h>

namespace Spieler::Math
{

    struct SphericalVector
    {
        float Theta{ 0.0f };    // XZ
        float Phi{ 0.0f };      // Y
        float Radius{ 1.0f };
    };

    inline DirectX::XMVECTOR SphericalToCartesian(float theta, float phi, float radius = 1.0f)
    {
        return DirectX::XMVectorSet(
            radius * DirectX::XMScalarSin(phi) * DirectX::XMScalarSin(theta),
            radius * DirectX::XMScalarCos(phi),
            radius * DirectX::XMScalarSin(phi) * DirectX::XMScalarCos(theta),
            0.0f
        );
    }

    inline DirectX::XMVECTOR SphericalToCartesian(const SphericalVector& sphericalVector)
    {
        return SphericalToCartesian(sphericalVector.Theta, sphericalVector.Phi, sphericalVector.Radius);
    }

    inline DirectX::XMVECTOR PitchYawRollToCartesian(float pitch, float yaw, float roll)
    {
        return DirectX::XMVectorSet(
            DirectX::XMScalarSin(yaw) * DirectX::XMScalarCos(pitch),
            DirectX::XMScalarSin(pitch),
            DirectX::XMScalarCos(yaw) * DirectX::XMScalarCos(pitch),
            1.0f
        );
    }

    template <typename T>
    inline T Clamp(const T& min, const T& value, const T& max)
    {
        return std::min<T>(std::max<T>(min, value), max);
    }

    inline DirectX::XMMATRIX GetRotationMatrixFromUnitDirectionVector(const DirectX::XMVECTOR& direction)
    {
#if 1
        const DirectX::XMVECTOR up{ DirectX::XMVectorSet(0.0, 1.0f, 0.0f, 0.0f) };
        const DirectX::XMVECTOR xaxis{ DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, direction)) };
        const DirectX::XMVECTOR yaxis{ DirectX::XMVector3Normalize(DirectX::XMVector3Cross(direction, xaxis)) };

        DirectX::XMFLOAT3 xaxis3;
        DirectX::XMFLOAT3 yaxis3;
        DirectX::XMFLOAT3 direction3;

        DirectX::XMStoreFloat3(&xaxis3, xaxis);
        DirectX::XMStoreFloat3(&yaxis3, yaxis);
        DirectX::XMStoreFloat3(&direction3, direction);

        DirectX::XMMATRIX result;
        result.r[0] = DirectX::XMVectorSet(xaxis3.x, yaxis3.x, direction3.x, 0.0f);
        result.r[1] = DirectX::XMVectorSet(xaxis3.y, yaxis3.y, direction3.y, 0.0f);
        result.r[2] = DirectX::XMVectorSet(xaxis3.z, yaxis3.z, direction3.z, 0.0f);
        result.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
#endif

#if 0
        DirectX::XMFLOAT3 float3;
        DirectX::XMStoreFloat3(&float3, direction);

        float c1 = std::sqrt(float3.x * float3.x + float3.y * float3.y);
        float s1 = float3.z;
        float c2 = c1 ? float3.x / c1 : 1.0;
        float s2 = c1 ? float3.y / c1 : 0.0;

        DirectX::XMMATRIX result;
        result.r[0] = DirectX::XMVectorSet(float3.x, -s2, -s1 * c2, 0.0f);
        result.r[1] = DirectX::XMVectorSet(float3.y, c2, -s1 * s2, 0.0f);
        result.r[2] = DirectX::XMVectorSet(float3.z, 0, c1, 0.0f);
        result.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
#endif

        return result;
    }

} // namespace Spieler::Math