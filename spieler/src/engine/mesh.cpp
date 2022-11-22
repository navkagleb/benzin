#include "spieler/config/bootstrap.hpp"

#include "spieler/engine/mesh.hpp"

namespace spieler
{

    MeshGeometry::MeshGeometry(Device& device, const std::unordered_map<std::string, MeshData>& submeshes)
    {
        SetSubmeshes(device, submeshes);
    }

    void MeshGeometry::SetSubmeshes(Device& device, const std::unordered_map<std::string, MeshData>& submeshes)
    {
        const uint32_t vertexSize{ sizeof(Vertex) };
        const uint32_t indexSize{ sizeof(uint32_t) };

        size_t vertexOffset{ 0 };
        size_t indexOffset{ 0 };

        for (const auto& [name, submesh] : submeshes)
        {
            m_Submeshes[name] = spieler::SubmeshGeometry
            {
                .IndexCount{ static_cast<uint32_t>(submesh.Indices.size()) },
                .BaseVertexLocation{ static_cast<uint32_t>(vertexOffset) },
                .StartIndexLocation{ static_cast<uint32_t>(indexOffset) }
            };

            m_Vertices.resize(m_Vertices.size() + submesh.Vertices.size());
            m_Indices.resize(m_Indices.size() + submesh.Indices.size());

            memcpy_s(m_Vertices.data() + vertexOffset, submesh.Vertices.size() * vertexSize, submesh.Vertices.data(), submesh.Vertices.size() * vertexSize);
            memcpy_s(m_Indices.data() + indexOffset, submesh.Indices.size() * indexSize, submesh.Indices.data(), submesh.Indices.size() * indexSize);

            // TODO: add ability to choose None | BoundingBox | BoundingSphere
            DirectX::BoundingBox::CreateFromPoints(
                m_Submeshes[name].BoundingBox,
                submesh.Vertices.size(),
                reinterpret_cast<const DirectX::XMFLOAT3*>(submesh.Vertices.data()),
                sizeof(spieler::Vertex)
            );

            vertexOffset += submesh.Vertices.size();
            indexOffset += submesh.Indices.size();
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
