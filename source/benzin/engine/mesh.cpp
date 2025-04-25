#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/mesh.hpp>

#include <shaders/joint/mesh_types.hpp>

#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/device.hpp>

namespace benzin
{

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

        return meshGpuStorage;
    }

}
