#include "benzin/config/bootstrap.hpp"
#include "benzin/core/math.hpp"

#include <shaders/joint/structured_buffer_types.hpp>

namespace benzin
{

    DirectX::XMVECTOR GetDirectionFromPitchYaw(float pitch, float yaw)
    {
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

} // namespace benzin
