#pragma once

namespace benzin
{

    class Model;

    struct ModelComponent
    {
        std::shared_ptr<Model> Model;
        std::optional<uint32_t> DrawPrimitiveIndex;
    };

    struct TransformComponent
    {
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

        DirectX::XMMATRIX GetInverseMatrix() const
        {
            const DirectX::XMMATRIX transform = GetMatrix();
            DirectX::XMVECTOR transformDeterminant = DirectX::XMMatrixDeterminant(transform);

            return DirectX::XMMatrixInverse(&transformDeterminant, transform);
        }
    };

    struct DirectionalLightComponent
    {
        DirectX::XMFLOAT3 Direction;
        DirectX::XMFLOAT3 Color;
        float Intensity;
    };

    struct PointLightComponent
    {
        DirectX::XMFLOAT3 Color;
        float Intensity;
        float Range;
    };

} // namespace benzin
