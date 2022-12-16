#include "spieler/config/bootstrap.hpp"

#include "spieler/engine/mesh.hpp"

namespace spieler
{

    Mesh::Mesh(Device& device, const std::unordered_map<std::string, MeshData>& subMeshes)
    {
        SetSubMeshes(device, subMeshes);
    }

    void Mesh::SetSubMeshes(Device& device, const std::unordered_map<std::string, MeshData>& subMeshes)
    {
        const uint32_t vertexSize = sizeof(Vertex);
        const uint32_t indexSize = sizeof(uint32_t);

        size_t vertexOffset = 0;
        size_t indexOffset = 0;

        for (const auto& [name, meshData] : subMeshes)
        {
            m_SubMeshes[name] = spieler::SubMesh
            {
                .IndexCount = static_cast<uint32_t>(meshData.Indices.size()),
                .BaseVertexLocation = static_cast<uint32_t>(vertexOffset),
                .StartIndexLocation = static_cast<uint32_t>(indexOffset)
            };

            m_Vertices.resize(m_Vertices.size() + meshData.Vertices.size());
            m_Indices.resize(m_Indices.size() + meshData.Indices.size());

            memcpy_s(m_Vertices.data() + vertexOffset, meshData.Vertices.size() * vertexSize, meshData.Vertices.data(), meshData.Vertices.size() * vertexSize);
            memcpy_s(m_Indices.data() + indexOffset, meshData.Indices.size() * indexSize, meshData.Indices.data(), meshData.Indices.size() * indexSize);

            // TODO: add ability to choose None | BoundingBox | BoundingSphere
            DirectX::BoundingBox::CreateFromPoints(
                m_SubMeshes[name].BoundingBox,
                meshData.Vertices.size(),
                reinterpret_cast<const DirectX::XMFLOAT3*>(meshData.Vertices.data()),
                sizeof(spieler::Vertex)
            );

            vertexOffset += meshData.Vertices.size();
            indexOffset += meshData.Indices.size();
        }

        // VertexBuffer
        {
            const BufferResource::Config config
            {
                .ElementSize{ vertexSize },
                .ElementCount{ static_cast<uint32_t>(m_Vertices.size()) },
            };

            m_VertexBuffer.SetBufferResource(spieler::BufferResource::Create(device, config));
        }

        // IndexBuffer
        {
            const BufferResource::Config config
            {
                .ElementSize{ indexSize },
                .ElementCount{ static_cast<uint32_t>(m_Indices.size()) }
            };

            m_IndexBuffer.SetBufferResource(spieler::BufferResource::Create(device, config));
        }
    }

} // namespace spieler
