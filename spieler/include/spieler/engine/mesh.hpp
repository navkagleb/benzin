#pragma once

#include "spieler/graphics/common.hpp"
#include "spieler/graphics/buffer.hpp"
#include "spieler/graphics/texture.hpp"
#include "spieler/graphics/device.hpp"

#include "spieler/engine/geometry_generator.hpp"

namespace spieler
{

    struct SubMesh
    {
        uint32_t IndexCount{ 0 };
        uint32_t BaseVertexLocation{ 0 };
        uint32_t StartIndexLocation{ 0 };

        DirectX::BoundingBox BoundingBox{};
    };

    class Mesh
    {
    public:
        Mesh() = default;
        explicit Mesh(Device& device, const std::unordered_map<std::string, MeshData>& subMeshes);

    public:
        const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
        const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

        Buffer& GetVertexBuffer() { return m_VertexBuffer; }
        const Buffer& GetVertexBuffer() const { return m_VertexBuffer; }

        Buffer& GetIndexBuffer() { return m_IndexBuffer; }
        const Buffer& GetIndexBuffer() const { return m_IndexBuffer; }

        const SubMesh& GetSubMesh(const std::string& name) const { return m_SubMeshes.at(name); }
        void SetSubMeshes(Device& device, const std::unordered_map<std::string, MeshData>& subMeshes);

    private:
        std::vector<Vertex> m_Vertices;
        std::vector<uint32_t> m_Indices;

        Buffer m_VertexBuffer;
        Buffer m_IndexBuffer;

        std::unordered_map<std::string, SubMesh> m_SubMeshes;
    };

    struct Material
    {
        DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
        float Roughness{ 0.25f };
        DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
        uint32_t DiffuseMapIndex{ 0 };
        uint32_t NormalMapIndex{ 0 };
        uint32_t StructuredBufferIndex{ 0 };
    };

    struct MeshComponent
    {
        const Mesh* Mesh{ nullptr };
        const SubMesh* SubMesh{ nullptr };
        PrimitiveTopology PrimitiveTopology{ PrimitiveTopology::Unknown };
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

    struct InstanceComponent
    {
        Transform Transform;
        uint32_t MaterialIndex{ 0 };
        bool IsVisible{ true };
    };

    struct InstancesComponent
    {
        std::vector<InstanceComponent> Instances;
        uint32_t VisibleInstanceCount{ 0 };
        uint32_t StructuredBufferOffset{ 0 };
        bool IsNeedCulling{ false };
    };

    class Entity
    {
    public:
        template <typename T>
        T& CreateComponent()
        {
            const uint64_t key = typeid(T).hash_code();

            SPIELER_ASSERT(!HasComponent<T>());

            m_Components[key] = T{};

            return std::any_cast<T&>(m_Components[key]);
        }

        template <typename T>
        T& GetComponent()
        {
            const uint64_t key = typeid(T).hash_code();

            SPIELER_ASSERT(HasComponent<T>());

            return std::any_cast<T&>(m_Components[key]);
        }

        template <typename T>
        const T& GetComponent() const
        {
            const uint64_t key = typeid(T).hash_code();

            SPIELER_ASSERT(HasComponent<T>());

            return std::any_cast<const T&>(m_Components.at(key));
        }

        template <typename T>
        bool HasComponent() const
        {
            return m_Components.contains(typeid(T).hash_code());
        }

    private:
        std::unordered_map<uint64_t, std::any> m_Components;
    };

} // namespace spieler
