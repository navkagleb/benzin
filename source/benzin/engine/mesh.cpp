#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/mesh.hpp>

#include <shaders/joint/mesh_types.hpp>

#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/device.hpp>

namespace benzin
{

    std::span<const joint::MeshVertex> Mesh::GetDrawRangeVertices(const MeshDrawRange& drawRange) const
    {
        return ToSpan(Vertices.data() + drawRange.VertexRange.Offset, drawRange.VertexRange.Count);
    }
    
    std::span<const uint32_t> Mesh::GetDrawRangeIndices(const MeshDrawRange& drawRange) const
    {
        return ToSpan(Indices.data() + drawRange.IndexRange.Offset, drawRange.IndexRange.Count);
    }

    MeshGpuStorage Mesh::CreateGpuStorage(Device& device, std::string_view debugName) const
    {
        MeshGpuStorage meshGpuStorage;

        MakeUniquePtr(meshGpuStorage.VertexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_VertexBuffer", debugName),
            .Type = BufferType::Vertex,
            .ElementSizeInBytes = sizeof(decltype(Vertices)::value_type),
            .ElementCount = (uint32_t)Vertices.size(),
        });

        MakeUniquePtr(meshGpuStorage.IndexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_IndexBuffer", debugName),
            .Type = BufferType::Index,
            .Format = GraphicsFormat::R32Uint,
            .ElementSizeInBytes = sizeof(uint32_t),
            .ElementCount = (uint32_t)Indices.size(),
        });

        MakeUniquePtr(meshGpuStorage.ObjectToLocalMatrixBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_ObjectToLocalMatrixBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSizeInBytes = sizeof(DirectX::XMMATRIX),
            .ElementCount = (uint32_t)Instances.size(),
        });

        MakeUniquePtr(meshGpuStorage.MeshletBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MeshletBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSizeInBytes = sizeof(decltype(Meshlets)::value_type),
            .ElementCount = (uint32_t)Meshlets.size(),
        });

        MakeUniquePtr(meshGpuStorage.MeshletCullVolumeBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MeshletCullVolumeBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSizeInBytes = sizeof(decltype(MeshletCullVolumes)::value_type),
            .ElementCount = (uint32_t)MeshletCullVolumes.size(),
        });

        MakeUniquePtr(meshGpuStorage.MeshletIndirectVertexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MeshletIndirectVertexBuffer", debugName),
            .Type = BufferType::Format,
            .Format = GraphicsFormat::R32Uint,
            .ElementSizeInBytes = sizeof(decltype(MeshletIndirectVertices)::value_type),
            .ElementCount = (uint32_t)MeshletIndirectVertices.size(),
        });

        MakeUniquePtr(meshGpuStorage.MeshletIndexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MeshletIndexBuffer", debugName),
            .Type = BufferType::Format,
            .Format = GraphicsFormat::R8Uint,
            .ElementSizeInBytes = sizeof(decltype(MeshletIndices)::value_type),
            .ElementCount = (uint32_t)MeshletIndices.size(),
        });

        return meshGpuStorage;
    }

}
