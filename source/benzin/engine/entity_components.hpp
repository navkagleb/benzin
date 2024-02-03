#pragma once

namespace benzin
{

    class Buffer;

    struct MeshInstanceComponent
    {
        uint32_t MeshCollectionIndex = g_InvalidIndex<uint32_t>;
        std::optional<std::pair<uint32_t, uint32_t>> MeshInstanceRange;
    };

    struct TransformComponent
    {
        std::unique_ptr<Buffer> Buffer;

        DirectX::XMFLOAT3 Scale{ 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 Rotation{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Translation{ 0.0f, 0.0f, 0.0f };

        DirectX::XMMATRIX GetMatrix() const
        {
            const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(Rotation.x) * DirectX::XMMatrixRotationY(Rotation.y) * DirectX::XMMatrixRotationZ(Rotation.z);
            const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z);
            const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(Translation.x, Translation.y, Translation.z);

            return scaling * rotation * translation;
        }
    };

    struct PointLightComponent
    {
        DirectX::XMFLOAT3 Color;
        float Intensity;
        float Range;
    };

} // namespace benzin
