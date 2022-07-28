#pragma once

#include "spieler/renderer/common.hpp"
#include "spieler/renderer/vertex_buffer.hpp"
#include "spieler/renderer/index_buffer.hpp"
#include "spieler/renderer/resource_view.hpp"

namespace spieler::renderer
{

    struct SubmeshGeometry
    {
        uint32_t IndexCount{ 0 };
        uint32_t BaseVertexLocation{ 0 };
        uint32_t StartIndexLocation{ 0 };
    };

    struct MeshGeometry
    {
        VertexBuffer VertexBuffer;
        IndexBuffer IndexBuffer;

        std::unordered_map<std::string, SubmeshGeometry> Submeshes;
    };

    struct Transform
    {
        DirectX::XMFLOAT3 Scale{ 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 Rotation{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Translation{ 0.0f, 0.0f, 0.0f };

        DirectX::XMMATRIX GetMatrix() const
        {
            const DirectX::XMMATRIX scaling{ DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z) };
            const DirectX::XMMATRIX rotation{ DirectX::XMMatrixRotationX(Rotation.x) * DirectX::XMMatrixRotationY(Rotation.y) * DirectX::XMMatrixRotationZ(Rotation.z) };
            const DirectX::XMMATRIX translation{ DirectX::XMMatrixTranslation(Translation.x, Translation.y, Translation.z) };

            return scaling * rotation * translation;
        }
    };

    struct MaterialConstants
    {
        DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
        float Roughness{ 0.25f };
    };

    struct Material
    {
        ShaderResourceView DiffuseMap;
        MaterialConstants Constants;
        Transform Transform;
    };

    struct RenderItem
    {
        const MeshGeometry* MeshGeometry{ nullptr };
        const SubmeshGeometry* SubmeshGeometry{ nullptr };
        PrimitiveTopology PrimitiveTopology{ PrimitiveTopology::Unknown };
        Material* Material{ nullptr };

        struct Transform Transform;
        struct Transform TextureTransform;
        
        template <typename T>
        T& GetComponent()
        {
            const uint64_t key{ typeid(T).hash_code() };

            if (!m_Components.contains(key))
            {
                m_Components[key] = T{};
            }

            return std::any_cast<T&>(m_Components[key]);
        }

        template <typename T>
        bool HasComponent() const
        {
            return m_Components.contains(typeid(T).hash_code());
        }

    private:
        std::unordered_map<uint64_t, std::any> m_Components;
    };

} // namespace spieler::renderer
