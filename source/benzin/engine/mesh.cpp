#include "benzin/config/bootstrap.hpp"

#include "benzin/engine/mesh.hpp"

#include "benzin/graphics/api/resource_view_builder.hpp"

namespace benzin
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

        std::string combinedName;

        for (const auto& [name, meshData] : subMeshes)
        {
            m_SubMeshes[name] = SubMesh
            {
                .VertexCount{ static_cast<uint32_t>(meshData.Vertices.size()) },
                .IndexCount{ static_cast<uint32_t>(meshData.Indices.size()) },
                .BaseVertexLocation{ static_cast<uint32_t>(vertexOffset) },
                .StartIndexLocation{ static_cast<uint32_t>(indexOffset) }
            };

            m_Vertices.resize(m_Vertices.size() + meshData.Vertices.size());
            m_Indices.resize(m_Indices.size() + meshData.Indices.size());

            memcpy_s(m_Vertices.data() + vertexOffset, meshData.Vertices.size() * vertexSize, meshData.Vertices.data(), meshData.Vertices.size() * vertexSize);
            memcpy_s(m_Indices.data() + indexOffset, meshData.Indices.size() * indexSize, meshData.Indices.data(), meshData.Indices.size() * indexSize);

            vertexOffset += meshData.Vertices.size();
            indexOffset += meshData.Indices.size();

            combinedName += "_" + name;
        }

        auto& resourceViewBuilder = device.GetResourceViewBuilder();

        // VertexBuffer
        {
            const BufferResource::Config config
            {
                .ElementSize{ vertexSize },
                .ElementCount{ static_cast<uint32_t>(m_Vertices.size()) },
            };

            m_VertexBuffer = device.CreateBufferResource(config, "VertexBuffer" + combinedName);
            m_VertexBuffer->PushShaderResourceView(resourceViewBuilder.CreateShaderResourceView(*m_VertexBuffer));
        }

        // IndexBuffer
        {
            const BufferResource::Config config
            {
                .ElementSize{ indexSize },
                .ElementCount{ static_cast<uint32_t>(m_Indices.size()) }
            };

            m_IndexBuffer = device.CreateBufferResource(config, "IndexBuffer" + combinedName);
            m_IndexBuffer->PushShaderResourceView(resourceViewBuilder.CreateShaderResourceView(*m_IndexBuffer));
        }
    }

} // namespace benzin
