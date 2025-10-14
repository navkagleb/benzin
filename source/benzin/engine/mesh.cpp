#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/mesh.hpp>

#include <shaders/joint/mesh_types.hpp>

#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>

namespace benzin
{

    std::span<const joint::MeshVertex> Mesh::GetDrawRangeVertices(const MeshDrawRange& drawRange) const
    {
        return ToSpan(m_Vertices.data() + drawRange.m_VertexRange.m_Offset, drawRange.m_VertexRange.m_Count);
    }
    
    std::span<const uint32_t> Mesh::GetDrawRangeIndices(const MeshDrawRange& drawRange) const
    {
        return ToSpan(m_Indices.data() + drawRange.m_IndexRange.m_Offset, drawRange.m_IndexRange.m_Count);
    }

    MeshGpuStorage Mesh::CreateGpuStorage(Device& device, std::string_view debugName) const
    {
        GpuHeapLinearBufferAllocator& allocator = device.GetPersistentDefaultLinearAllocator();

        MeshGpuStorage meshGpuStorage;
        meshGpuStorage.VertexBuffer = allocator.AllocateBuffer(std::format("{}_VertexBuffer", debugName), ToSpan(m_Vertices));
        meshGpuStorage.IndexBuffer = allocator.AllocateBuffer(std::format("{}_IndexBuffer", debugName), ToSpan(m_Indices), GraphicsFormat::R32Uint);
        meshGpuStorage.MeshletBuffer = allocator.AllocateBuffer(std::format("{}_MeshletBuffer", debugName), ToSpan(m_Meshlets));
        meshGpuStorage.MeshletCullVolumeBuffer = allocator.AllocateBuffer(std::format("{}_MeshletCullVolumeBuffer", debugName), ToSpan(m_MeshletCullVolumes));
        meshGpuStorage.MeshletIndirectVertexBuffer = allocator.AllocateBuffer(std::format("{}_MeshletIndirectVertexBuffer", debugName), ToSpan(m_MeshletIndirectVertices), GraphicsFormat::R32Uint);
        meshGpuStorage.MeshletIndexBuffer = allocator.AllocateBuffer(std::format("{}_MeshletIndexBuffer", debugName), ToSpan(m_MeshletIndices), GraphicsFormat::R8Uint);

        return meshGpuStorage;
    }

}
