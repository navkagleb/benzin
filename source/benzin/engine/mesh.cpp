#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/mesh.hpp>

#include <shaders/joint/mesh_types.hpp>

#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/device.hpp>

namespace benzin
{

    std::span<const joint::MeshVertex> Mesh::GetDrawRangeVertices(const MeshDrawRange& drawRange) const
    {
        return ToSpan(Vertices.data() + drawRange.VertexOffset, drawRange.VertexCount);
    }
    
    std::span<const uint32_t> Mesh::GetDrawRangeIndices(const MeshDrawRange& drawRange) const
    {
        return ToSpan(Indices.data() + drawRange.IndexOffset, drawRange.IndexCount);
    }

    MeshGpuStorage Mesh::CreateGpuStorage(Device& device, std::string_view debugName) const
    {
        MeshGpuStorage meshGpuStorage;

        MakeUniquePtr(meshGpuStorage.VertexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_VertexBuffer", debugName),
            .Type = BufferType::Vertex,
            .ElementSizeInBytes = sizeof(joint::MeshVertex),
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

        MakeUniquePtr(meshGpuStorage.InstanceTransformBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_InstanceTransformBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSizeInBytes = sizeof(DirectX::XMMATRIX),
            .ElementCount = (uint32_t)Instances.size(),
        });

        MakeUniquePtr(meshGpuStorage.MeshletBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MeshletBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSizeInBytes = sizeof(joint::Meshlet),
            .ElementCount = (uint32_t)Meshlets.size(),
        });

        MakeUniquePtr(meshGpuStorage.MeshletVertexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MeshletVertexBuffer", debugName),
            .Type = BufferType::Format,
            .Format = GraphicsFormat::R32Uint,
            .ElementSizeInBytes = sizeof(uint32_t),
            .ElementCount = (uint32_t)MeshletVertices.size(),
        });

        MakeUniquePtr(meshGpuStorage.MeshletTriangleBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MeshletTriangleBuffer", debugName),
            .Type = BufferType::Format,
            .Format = GraphicsFormat::R8Uint,
            .ElementSizeInBytes = sizeof(uint8_t),
            .ElementCount = (uint32_t)MeshletTriangles.size(),
        });

        return meshGpuStorage;
    }

}
