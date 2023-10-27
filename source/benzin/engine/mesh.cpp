#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/mesh.hpp"

#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    namespace
    {

        struct GPUSubMesh
        {
            uint32_t StartVertex;
            uint32_t StartIndex;
            uint32_t IndexCount;
        };

    } // anonymous namespace

    Mesh::Mesh(Device& device)
        : m_Device{ device }
    {}

    Mesh::Mesh(Device& device, const MeshCreation& creation)
        : Mesh{ device }
    {
        Create(creation);
    }

    void Mesh::Create(const MeshCreation& creation)
    {
        BenzinAssert(!creation.DebugName.empty());
        m_DebugName = creation.DebugName;

        // TODO: Is validation necessary?
#if 0
        if (creation.Meshes.size() == 1)
        {
            const_cast<MeshCreation&>(creation).IsNeedSplitByMeshes = false;
        }
#endif

        uint32_t totalVertexCount = 0;
        uint32_t totalIndexCount = 0;
        for (const auto& mesh : creation.Meshes)
        {
            totalVertexCount += static_cast<uint32_t>(mesh.Vertices.size());
            totalIndexCount += static_cast<uint32_t>(mesh.Indices.size());
        }

        m_VertexBuffer = std::make_shared<Buffer>(m_Device, BufferCreation
        {
            .DebugName = std::format("{}_{}", m_DebugName, "VertexBuffer"),
            .ElementSize = sizeof(MeshVertex),
            .ElementCount = totalVertexCount,
            .IsNeedShaderResourceView = true,
        });

        m_IndexBuffer = std::make_shared<Buffer>(m_Device, BufferCreation
        {
            .DebugName = std::format("{}_{}", m_DebugName, "IndexBuffer"),
            .ElementSize = sizeof(uint32_t),
            .ElementCount = totalIndexCount,
            .IsNeedShaderResourceView = true,
        });

        if (creation.IsNeedSplitByMeshes)
        {
            m_SubMeshBuffer = std::make_shared<Buffer>(m_Device, BufferCreation
            {
                .DebugName = std::format("{}_{}", m_DebugName, "SubMeshBuffer"),
                .ElementSize = sizeof(GPUSubMesh),
                .ElementCount = static_cast<uint32_t>(creation.Meshes.size()),
                .IsNeedShaderResourceView = true,
            });
        }

        // Fill buffers
        uint32_t uploadBufferSize = m_VertexBuffer->GetSizeInBytes() + m_IndexBuffer->GetSizeInBytes();
        if (creation.IsNeedSplitByMeshes)
        {
            uploadBufferSize += m_SubMeshBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        for (const auto [i, mesh] : creation.Meshes | std::views::enumerate)
        {
            const auto meshVertexCount = static_cast<uint32_t>(mesh.Vertices.size());
            const auto meshIndexCount = static_cast<uint32_t>(mesh.Indices.size());

            m_SubMeshes.emplace_back(meshIndexCount, mesh.PrimitiveTopology);

            copyCommandList.UpdateBuffer(*m_VertexBuffer, std::span{ mesh.Vertices }, vertexOffset);
            copyCommandList.UpdateBuffer(*m_IndexBuffer, std::span{ mesh.Indices }, indexOffset);

            if (creation.IsNeedSplitByMeshes)
            {
                const GPUSubMesh gpuSubMesh
                {
                    .StartVertex = vertexOffset,
                    .StartIndex = indexOffset,
                    .IndexCount = meshIndexCount,
                };

                copyCommandList.UpdateBuffer(*m_SubMeshBuffer, std::span{ &gpuSubMesh, 1 }, i);
            }

            vertexOffset += meshVertexCount;
            indexOffset += meshIndexCount;
        }
    }

} // namespace benzin
