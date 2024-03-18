#pragma once

namespace joint
{

    struct MeshVertex;

} // namespace joint

namespace benzin
{

    DirectX::XMVECTOR GetDirectionFromPitchYaw(float pitch, float yaw);
    DirectX::XMFLOAT2 GetPitchYawFromDirection(const DirectX::XMVECTOR& direction);

    DirectX::BoundingBox ComputeBoundingBox(std::span<const joint::MeshVertex> vertices);
    DirectX::BoundingBox TransformBoundingBox(const DirectX::BoundingBox& boundingBox, const DirectX::XMMATRIX& transformMatrix);

    DirectX::XMMATRIX GetMatrixForNormals(const DirectX::XMMATRIX& transform);

} // namespace benzin
