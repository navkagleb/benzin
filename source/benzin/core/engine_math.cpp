#include "benzin/config/bootstrap.hpp"
#include "benzin/core/engine_math.hpp"

#include <shaders/joint/structured_buffer_types.hpp>

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
            0.0f
        ));
    }

    DirectX::XMFLOAT2 GetPitchYawFromDirection(const DirectX::XMVECTOR& direction)
    {
        DirectX::XMFLOAT3 unpackedDirection;
        DirectX::XMStoreFloat3(&unpackedDirection, direction);

        float pitch = std::asin(unpackedDirection.y);
        float yaw = std::atan2(unpackedDirection.z, unpackedDirection.x);

        return DirectX::XMFLOAT2{ pitch, yaw };
    }

    DirectX::BoundingBox ComputeBoundingBox(std::span<const joint::MeshVertex> vertices)
    {
        DirectX::BoundingBox boundingBox;
        DirectX::BoundingBox::CreateFromPoints(boundingBox, vertices.size(), (const DirectX::XMFLOAT3*)vertices.data(), sizeof(joint::MeshVertex));

        return boundingBox;
    }

    DirectX::BoundingBox TransformBoundingBox(const DirectX::BoundingBox& boundingBox, const DirectX::XMMATRIX& transformMatrix)
    {
        DirectX::BoundingBox transformedBoundingBox;
        boundingBox.Transform(transformedBoundingBox, transformMatrix);

        return transformedBoundingBox;
    }

    DirectX::XMMATRIX GetMatrixForNormals(const DirectX::XMMATRIX& transform)
    {
        return DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, transform));
    }

    float GetWeylSequence(float seed, uint32_t n)
    {
        // Ref: https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
        // [0, 1)

        float integerPart;
        return std::modf(seed + (float)(n * 10368889) / std::exp2(24.0f), &integerPart);
    }

    DirectX::XMFLOAT4 GetRotator(float angleInRadians)
    {
        // Ref: https://en.wikipedia.org/wiki/Rotation_matrix
        // This is 2x2 rotation matrix

        const float cosAngle = DirectX::XMScalarCos(angleInRadians);
        const float sinAngle = DirectX::XMScalarSin(angleInRadians);

        // TODO: Do I need to transpose it?
        return DirectX::XMFLOAT4{ cosAngle, sinAngle, -sinAngle, cosAngle };
    }

}
