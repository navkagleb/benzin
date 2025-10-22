#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/ray_tracing_acceleration_structures.hpp"

#include "benzin/core/buffer_writer.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/gpu_heap.hpp"

namespace benzin
{

    // RayTracing_AcclerationStructure

    RayTracing_AcclerationStructure::~RayTracing_AcclerationStructure() = default;

    uint64_t RayTracing_AcclerationStructure::GetGpuVirtualAddress() const
    {
        return m_Buffer->GetGpuVirtualAddress();
    }

    void RayTracing_AcclerationStructure::AllocateBuffers(Device& device, std::string_view debugName, const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& d3d12BuildInputs)
    {
        m_D3D12BuildInputs = d3d12BuildInputs;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO d3d12PrebuildInfo{};
        device.GetD3D12Device()->GetRaytracingAccelerationStructurePrebuildInfo(&m_D3D12BuildInputs, &d3d12PrebuildInfo);
        BenzinEnsure(d3d12PrebuildInfo.ResultDataMaxSizeInBytes > 0);

        const bool isTlas = m_D3D12BuildInputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        const std::string_view typeName = isTlas ? "TLAS" : "BLAS";

        MakeUniquePtr(m_Buffer, device, BufferCreation
        {
            .DebugName = std::format("{}_AccelerationStructure_{}", typeName, debugName),
            .HeapType = GpuHeapType::Default,
            .Type = BufferType::RayTracing_AccelerationStructure,
            .ElementCount = (uint32_t)d3d12PrebuildInfo.ResultDataMaxSizeInBytes,
            .IsUnorderedAccessAllowed = true,
        });

        MakeUniquePtr(m_ScratchResource, device, BufferCreation
        {
            .DebugName = std::format("{}_ScratchResource_{}", typeName, debugName),
            .HeapType = GpuHeapType::Default,
            .ElementCount = (uint32_t)d3d12PrebuildInfo.ScratchDataSizeInBytes,
            .IsUnorderedAccessAllowed = true,
        });
    }

    // RayTracing_Blas

    RayTracing_Blas::RayTracing_Blas(uint32_t reservedGeometryCount)
    {
        m_D3D12GeometryDescs.reserve(reservedGeometryCount);
    }

    void RayTracing_Blas::AddGeometry(const Geometry& geometry)
    {
        BenzinAssert(geometry.m_IndexBuffer.GetElementSizeInBytes() == sizeof(uint32_t));
        BenzinAssert(geometry.m_TransformGpuAddress % D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT == 0);

        D3D12_RAYTRACING_GEOMETRY_DESC d3d12GometryDesc = {};
        d3d12GometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        d3d12GometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        d3d12GometryDesc.Triangles.Transform3x4 = geometry.m_TransformGpuAddress;
        d3d12GometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        d3d12GometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        d3d12GometryDesc.Triangles.IndexCount = geometry.m_IndexCount;
        d3d12GometryDesc.Triangles.VertexCount = geometry.m_VertexCount;
        d3d12GometryDesc.Triangles.IndexBuffer = geometry.m_IndexBuffer.GetGpuVirtualAddress(geometry.m_IndexOffset);
        d3d12GometryDesc.Triangles.VertexBuffer.StartAddress = geometry.m_VertexBuffer.GetGpuVirtualAddress(geometry.m_VertexOffset);
        d3d12GometryDesc.Triangles.VertexBuffer.StrideInBytes = geometry.m_VertexBuffer.GetElementSizeInBytes();

        m_D3D12GeometryDescs.push_back(d3d12GometryDesc);
    }

    void RayTracing_Blas::AllocateBuffers(Device& device, std::string_view debugName)
    {
        BenzinAssert(!m_D3D12GeometryDescs.empty());

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12BuildInputs = {};
        d3d12BuildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        d3d12BuildInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        d3d12BuildInputs.NumDescs = (uint32_t)m_D3D12GeometryDescs.size();
        d3d12BuildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        d3d12BuildInputs.pGeometryDescs = m_D3D12GeometryDescs.data();

        RayTracing_AcclerationStructure::AllocateBuffers(device, debugName, d3d12BuildInputs);
    }

    // RayTracing_Tlas

    void RayTracing_Tlas::AddInstance(const Instance& instance)
    {
        // D3D12_RAYTRACING_INSTANCE_DESC::InstanceID - 24 bit
        // D3D12_RAYTRACING_INSTANCE_DESC::InstanceMask - 8 bit - Bitwise AND with TraceRay() parameter
        // D3D12_RAYTRACING_INSTANCE_DESC::InstanceContributionToHitGroupIndex - 24 bit - Chose hit group shader
        // D3D12_RAYTRACING_INSTANCE_DESC::Flags - 8 bit

        // TODO: Do I need D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE?

        D3D12_RAYTRACING_INSTANCE_DESC d3d12InstanceDesc = {};
        d3d12InstanceDesc.InstanceID = 0;
        d3d12InstanceDesc.InstanceMask = 1;
        d3d12InstanceDesc.InstanceContributionToHitGroupIndex = 0; // TODO: For now all instances have default hit group
        d3d12InstanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
        d3d12InstanceDesc.AccelerationStructure = instance.m_BlasGpuVirtualAddress;

        const DirectX::XMMATRIX localToWorld = DirectX::XMMatrixTranspose(instance.m_LocalToWorld);
        memcpy(&d3d12InstanceDesc.Transform, &localToWorld, sizeof(DirectX::XMFLOAT3X4));

        m_D3D12InstanceDescs.push_back(d3d12InstanceDesc);
    }

    void RayTracing_Tlas::ResetInstances(uint32_t reservedInstanceCount)
    {
        m_D3D12InstanceDescs.clear();
        m_D3D12InstanceDescs.reserve(reservedInstanceCount);
    }

    void RayTracing_Tlas::AllocateBuffers(Device& device, std::string_view debugName)
    {
        AllocateInstanceBuffer(device, debugName);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12BuildInputs = {};
        d3d12BuildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        d3d12BuildInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
        d3d12BuildInputs.NumDescs = (uint32_t)m_D3D12InstanceDescs.size();
        d3d12BuildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        d3d12BuildInputs.InstanceDescs = m_InstanceBuffer->GetGpuVirtualAddress();

        RayTracing_AcclerationStructure::AllocateBuffers(device, debugName, d3d12BuildInputs);
    }

    void RayTracing_Tlas::AllocateInstanceBuffer(Device& device, std::string_view debugName)
    {
        BenzinAssert(!m_D3D12InstanceDescs.empty());

        MakeUniquePtr(m_InstanceBuffer, device, BufferCreation
        {
            .DebugName = std::format("TLAS_InstanceBuffer_{}", debugName),
            .HeapType = GpuHeapType::Upload,// TODO: Remove UploadBuffer
            .ElementSizeInBytes = sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
            .ElementCount = (uint32_t)m_D3D12InstanceDescs.size(),
        });

        BufferWriter writer = MakeBufferWriter(*m_InstanceBuffer);
        writer.WriteArray(ToSpan(m_D3D12InstanceDescs));
    }

}
