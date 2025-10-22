#pragma once

namespace joint
{
    struct MeshVertex;
}

namespace benzin
{

    DirectX::XMVECTOR GetDirectionFromPitchYaw(float pitch, float yaw);
    DirectX::XMFLOAT2 GetPitchYawFromDirection(const DirectX::XMVECTOR& direction);

    DirectX::BoundingBox ComputeBoundingBox(std::span<const joint::MeshVertex> vertices);

    float GetWeylSequence(float seed, uint32_t n);

    DirectX::XMFLOAT4 GetRotator(float angleInRadians);
}
