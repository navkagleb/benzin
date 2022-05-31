#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/mesh.hpp"

namespace spieler::renderer
{

    SubmeshGeometry& MeshGeometry::CreateSubmesh(const std::string& name)
    {
        SPIELER_ASSERT(!m_Submeshes.contains(name));

        return m_Submeshes[name];
    }

    SubmeshGeometry& MeshGeometry::GetSubmesh(const std::string& name)
    {
        SPIELER_ASSERT(m_Submeshes.contains(name));

        return m_Submeshes[name];
    }

    const SubmeshGeometry& MeshGeometry::GetSubmesh(const std::string& name) const
    {
        SPIELER_ASSERT(m_Submeshes.contains(name));

        return m_Submeshes.at(name);
    }

    void MeshGeometry::GenerateFrom(const std::vector<NamedMeshData<>>& meshes, UploadBuffer& uploadBuffer)
    {
        SPIELER_ASSERT(!meshes.empty());

        uint32_t vertexOffset{ 0 };
        uint32_t indexOffset{ 0 };

        uint32_t vertexCount{ 0 };
        uint32_t indexCount{ 0 };

        for (const NamedMeshData<>& mesh : meshes)
        {
            SubmeshGeometry& submesh{ m_Submeshes[mesh.Name] };
            submesh.IndexCount = static_cast<std::uint32_t>(mesh.MeshData.Indices.size());
            submesh.BaseVertexLocation = vertexOffset;
            submesh.StartIndexLocation = indexOffset;

            vertexOffset += static_cast<uint32_t>(mesh.MeshData.Vertices.size());
            indexOffset += static_cast<uint32_t>(mesh.MeshData.Indices.size());
        }

        std::vector<Vertex> vertices;
        vertices.reserve(vertexOffset);

        std::vector<uint32_t> indices;
        indices.reserve(indexOffset);

        for (const auto& mesh : meshes)
        {
            vertices.insert(vertices.end(), mesh.MeshData.Vertices.begin(), mesh.MeshData.Vertices.end());
            indices.insert(indices.end(), mesh.MeshData.Indices.begin(), mesh.MeshData.Indices.end());
        }

        SPIELER_ASSERT(InitStaticVertexBuffer(vertices.data(), static_cast<uint32_t>(vertices.size()), uploadBuffer));
        SPIELER_ASSERT(InitStaticIndexBuffer(indices.data(), static_cast<uint32_t>(indices.size()), uploadBuffer));
    }


} // namespace spieler::renderer