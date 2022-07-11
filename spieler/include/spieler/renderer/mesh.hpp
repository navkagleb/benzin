#pragma once

#include "spieler/renderer/vertex_buffer.hpp"
#include "spieler/renderer/index_buffer.hpp"
#include "spieler/renderer/constant_buffer.hpp"
#include "spieler/renderer/texture.hpp"
#include "spieler/renderer/geometry_generator.hpp"
#include "spieler/renderer/renderer.hpp"

namespace spieler::renderer
{

    struct SubmeshGeometry
    {
        uint32_t IndexCount{ 0 };
        uint32_t BaseVertexLocation{ 0 };
        uint32_t StartIndexLocation{ 0 };
    };

    template <typename Index = uint32_t>
    struct NamedMeshData
    {
        std::string Name;
        MeshData<Index> MeshData;
    };

    class MeshGeometry
    {
    public:
        const VertexBuffer& GetVertexBuffer() const { return m_VertexBuffer; }
        const IndexBuffer& GetIndexBuffer() const { return m_IndexBuffer; }

    public:
        template <typename Vertex>
        bool InitStaticVertexBuffer(const Vertex* vertices, uint32_t vertexCount, UploadBuffer& uploadBuffer);

        template <typename Index>
        bool InitStaticIndexBuffer(const Index* indices, uint32_t indexCount, UploadBuffer& uploadBuffer);

        SubmeshGeometry& CreateSubmesh(const std::string& name);
        SubmeshGeometry& GetSubmesh(const std::string& name);
        const SubmeshGeometry& GetSubmesh(const std::string& name) const;

        void GenerateFrom(const std::vector<NamedMeshData<>>& meshes, UploadBuffer& uploadBuffer);

    public:
        // TODO: Move to private
        VertexBuffer m_VertexBuffer;
        IndexBuffer m_IndexBuffer;

        std::unordered_map<std::string, SubmeshGeometry> m_Submeshes;
    };

    template <typename Vertex>
    bool MeshGeometry::InitStaticVertexBuffer(const Vertex* vertices, uint32_t vertexCount, UploadBuffer& uploadBuffer)
    {
        auto& renderer{ Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };

        const BufferConfig bufferConfig
        {
            .ElementSize = sizeof(Vertex),
            .ElementCount = vertexCount
        };

        const BufferFlags bufferFlags{ BufferFlags::None};

        m_VertexBuffer.SetResource(device.CreateBuffer(bufferConfig, bufferFlags));
        
        spieler::renderer::Renderer::GetInstance().GetContext().CopyBuffer(vertices, bufferConfig.ElementSize * bufferConfig.ElementCount, uploadBuffer, *m_VertexBuffer.GetResource());

        return true;
    }

    template <typename Index>
    bool MeshGeometry::InitStaticIndexBuffer(const Index* indices, uint32_t indexCount, UploadBuffer& uploadBuffer)
    {
        auto& renderer{ Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };

        const BufferConfig bufferConfig
        {
            .ElementSize = sizeof(Index),
            .ElementCount = indexCount
        };

        const BufferFlags bufferFlags{ BufferFlags::None };

        m_IndexBuffer.SetResource(device.CreateBuffer(bufferConfig, bufferFlags));

        spieler::renderer::Renderer::GetInstance().GetContext().CopyBuffer(indices, bufferConfig.ElementSize * bufferConfig.ElementCount, uploadBuffer, *m_IndexBuffer.GetResource());

        return true;
    }

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
        MeshGeometry* MeshGeometry{ nullptr };
        SubmeshGeometry* SubmeshGeometry{ nullptr };
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