#pragma once

#include "spieler/renderer/common.hpp"
#include "spieler/renderer/buffer.hpp"
#include "spieler/renderer/texture.hpp"
#include "spieler/renderer/geometry_generator.hpp"

namespace spieler::renderer
{

    struct SubmeshGeometry
    {
        uint32_t IndexCount{ 0 };
        uint32_t BaseVertexLocation{ 0 };
        uint32_t StartIndexLocation{ 0 };

        DirectX::BoundingBox BoundingBox{};
    };

    class MeshGeometry
    {
    public:
        MeshGeometry() = default;
        explicit MeshGeometry(const std::unordered_map<std::string, MeshData>& submeshes);

    public:
        const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
        const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

        Buffer& GetVertexBuffer() { return m_VertexBuffer; }
        const Buffer& GetVertexBuffer() const { return m_VertexBuffer; }

        Buffer& GetIndexBuffer() { return m_IndexBuffer; }
        const Buffer& GetIndexBuffer() const { return m_IndexBuffer; }

        const SubmeshGeometry& GetSubmesh(const std::string& name) const { return m_Submeshes.at(name); }
        void SetSubmeshes(const std::unordered_map<std::string, MeshData>& submeshes);

    private:
        std::vector<Vertex> m_Vertices;
        std::vector<uint32_t> m_Indices;

        Buffer m_VertexBuffer;
        Buffer m_IndexBuffer;

        std::unordered_map<std::string, SubmeshGeometry> m_Submeshes;
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

        DirectX::XMMATRIX GetInverseMatrix() const
        {
            const DirectX::XMMATRIX transform{ GetMatrix() };
            DirectX::XMVECTOR transformDeterminant{ DirectX::XMMatrixDeterminant(transform) };

            return DirectX::XMMatrixInverse(&transformDeterminant, transform);
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
        TextureShaderResourceView DiffuseMap;
        MaterialConstants Constants;
        Transform Transform;
        uint32_t ConstantBufferIndex{ 0 };
    };

    struct RenderItem
    {
        const MeshGeometry* MeshGeometry{ nullptr };
        const SubmeshGeometry* SubmeshGeometry{ nullptr };
        PrimitiveTopology PrimitiveTopology{ PrimitiveTopology::Unknown };
        Material* Material{ nullptr };

        uint32_t ConstantBufferIndex{ 0 };

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
