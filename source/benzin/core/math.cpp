#include <benzin/config/bootstrap.hpp>
#include <benzin/core/math.hpp>

namespace benzin
{

    DirectX::XMVECTOR GetDirectionFromPitchYaw(float pitch, float yaw)
    {
        // Pitch - vertical angle (in radians)
        // Yaw - horizontal angle (in radians)

        return DirectX::XMVector3Normalize(DirectX::XMVectorSet(
            DirectX::XMScalarCos(yaw) * DirectX::XMScalarCos(pitch),
            DirectX::XMScalarSin(pitch),
            DirectX::XMScalarSin(yaw) * DirectX::XMScalarCos(pitch),
            0.0f));
    }

    DirectX::XMFLOAT2 GetPitchYawFromDirection(const DirectX::XMVECTOR& direction)
    {
        DirectX::XMFLOAT3 direction3;
        DirectX::XMStoreFloat3(&direction3, direction);

        float pitch = std::asin(direction3.y);
        float yaw = std::atan2(direction3.z, direction3.x);

        return DirectX::XMFLOAT2{ pitch, yaw };
    }

}
