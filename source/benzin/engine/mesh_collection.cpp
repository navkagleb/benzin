#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/mesh_collection.hpp"

#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    namespace
    {

        struct GPU_MeshInfo
        {
            uint32_t StartVertex;
            uint32_t StartIndex;
            uint32_t IndexCount;
        };

    } // anonymous namespace

    MeshCollection::MeshCollection(Device& device)
        : m_Device{ device }
    {}

    MeshCollection::MeshCollection(Device& device, const MeshCollectionCreation& creation)
        : MeshCollection{ device }
    {
        Create(creation);
    }

    void MeshCollection::Create(const MeshCollectionCreation& creation)
    {
        BenzinAssert(!creation.DebugName.empty());
        BenzinAssert(!creation.Meshes.empty());

        m_DebugName = creation.DebugName;

        uint32_t totalVertexCount = 0;
        uint32_t totalIndexCount = 0;
        for (const auto& mesh : creation.Meshes)
        {
            totalVertexCount += (uint32_t)mesh.Vertices.size();
            totalIndexCount += (uint32_t)mesh.Indices.size();
        }

        m_VertexBuffer = std::make_shared<Buffer>(m_Device, BufferCreation
        {
            .DebugName = std::format("{}_{}", m_DebugName, "VertexBuffer"),
            .ElementSize = sizeof(MeshVertex),
            .ElementCount = totalVertexCount,
            .IsNeedStructuredBufferView = creation.IsNeedDefaultBufferViews,
        });

        m_IndexBuffer = std::make_shared<Buffer>(m_Device, BufferCreation
        {
            .DebugName = std::format("{}_{}", m_DebugName, "IndexBuffer"),
            .ElementSize = sizeof(MeshIndex),
            .ElementCount = totalIndexCount,
            .IsNeedStructuredBufferView = creation.IsNeedDefaultBufferViews,
        });

        if (creation.IsNeedSplitByMeshes)
        {
            m_MeshInfoBuffer = std::make_shared<Buffer>(m_Device, BufferCreation
            {
                .DebugName = std::format("{}_{}", m_DebugName, "SubMeshBuffer"),
                .ElementSize = sizeof(GPU_MeshInfo),
                .ElementCount = (uint32_t)creation.Meshes.size(),
                .IsNeedStructuredBufferView = creation.IsNeedDefaultBufferViews,
            });
        }

        // Fill buffers
        uint32_t uploadBufferSize = m_VertexBuffer->GetSizeInBytes() + m_IndexBuffer->GetSizeInBytes();
        if (creation.IsNeedSplitByMeshes)
        {
            uploadBufferSize += m_MeshInfoBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        m_MeshInfos.reserve(creation.Meshes.size());

        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        for (const auto [i, mesh] : creation.Meshes | std::views::enumerate)
        {
            const auto meshVertexCount = (uint32_t)mesh.Vertices.size();
            const auto meshIndexCount = (uint32_t)mesh.Indices.size();

            m_MeshInfos.push_back(MeshInfo
            {
                .StartVertex = vertexOffset,
                .StartIndex = indexOffset,
                .IndexCount = meshIndexCount,
                .PrimitiveTopology = mesh.PrimitiveTopology,
            });

            copyCommandList.UpdateBuffer(*m_VertexBuffer, std::span{ mesh.Vertices }, vertexOffset);
            copyCommandList.UpdateBuffer(*m_IndexBuffer, std::span{ mesh.Indices }, indexOffset);

            if (creation.IsNeedSplitByMeshes)
            {
                const GPU_MeshInfo gpuMeshInfo
                {
                    .StartVertex = vertexOffset,
                    .StartIndex = indexOffset,
                    .IndexCount = meshIndexCount,
                };

                copyCommandList.UpdateBuffer(*m_MeshInfoBuffer, std::span{ &gpuMeshInfo, 1 }, i);
            }

            vertexOffset += meshVertexCount;
            indexOffset += meshIndexCount;
        }
    }

} // namespace benzin
