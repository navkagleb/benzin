#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/ray_tracing_acceleration_structures.hpp"

#include "benzin/core/buffer_writer.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"

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
            .Type = BufferType::RayTracing_AccelerationStructure,
            .ElementCount = (uint32_t)d3d12PrebuildInfo.ResultDataMaxSizeInBytes,
            .IsUnorderedAccessAllowed = true,
        });

        MakeUniquePtr(m_ScratchResource, device, BufferCreation
        {
            .DebugName = std::format("{}_ScratchResource_{}", typeName, debugName),
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
        const uint32_t validatedVertexCount = geometry.VertexRange.IsGoodRange() ? geometry.VertexRange.Count : (uint32_t)geometry.VertexBuffer.GetElementCount();
        const uint32_t validatedIndexCount = geometry.IndexRange.IsGoodRange() ? geometry.IndexRange.Count : (uint32_t)geometry.IndexBuffer.GetElementCount();

        BenzinAssert(validatedVertexCount + geometry.VertexRange.Offset <= geometry.VertexBuffer.GetElementCount());
        BenzinAssert(validatedIndexCount + geometry.IndexRange.Offset <= geometry.IndexBuffer.GetElementCount());
        BenzinAssert(geometry.TransformGpuAddress % D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT == 0);

        m_D3D12GeometryDescs.push_back(D3D12_RAYTRACING_GEOMETRY_DESC
        {
            .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
            .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
            .Triangles
            {
                .Transform3x4 = geometry.TransformGpuAddress,
                .IndexFormat = DXGI_FORMAT_R32_UINT,
                .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                .IndexCount = validatedIndexCount,
                .VertexCount = validatedVertexCount,
                .IndexBuffer = geometry.IndexBuffer.GetGpuVirtualAddress(geometry.IndexRange.Offset),
                .VertexBuffer
                {
                    .StartAddress = geometry.VertexBuffer.GetGpuVirtualAddress(geometry.VertexRange.Offset),
                    .StrideInBytes = geometry.VertexBuffer.GetElementSizeInBytes(),
                },
            },
        });
    }

    void RayTracing_Blas::AllocateBuffers(Device& device, std::string_view debugName)
    {
        BenzinAssert(!m_D3D12GeometryDescs.empty());

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12BuildInputs
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
            .NumDescs = (uint32_t)m_D3D12GeometryDescs.size(),
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .pGeometryDescs = m_D3D12GeometryDescs.data(),
        };

        RayTracing_AcclerationStructure::AllocateBuffers(device, debugName, d3d12BuildInputs);
    }

    // RayTracing_Tlas

    RayTracing_Tlas::RayTracing_Tlas(RayTracing_Tlas&& other) noexcept
    {
        if (this == &other)
        {
            return;
        }

        m_D3D12BuildInputs = other.m_D3D12BuildInputs;
        m_Buffer = std::exchange(other.m_Buffer, nullptr);
        m_ScratchResource = std::exchange(other.m_ScratchResource, nullptr);

        m_D3D12InstanceDescs = std::exchange(other.m_D3D12InstanceDescs, {});
        m_InstanceBuffer = std::exchange(other.m_InstanceBuffer, nullptr);
    }

    void RayTracing_Tlas::AddInstance(const Instance& instance)
    {
        // D3D12_RAYTRACING_INSTANCE_DESC::InstanceID - 24 bit
        // D3D12_RAYTRACING_INSTANCE_DESC::InstanceMask - 8 bit - Bitwise AND with TraceRay() parameter
        // D3D12_RAYTRACING_INSTANCE_DESC::InstanceContributionToHitGroupIndex - 24 bit - Chose hit group shader
        // D3D12_RAYTRACING_INSTANCE_DESC::Flags - 8 bit

        // TODO: Do I need D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE?

        D3D12_RAYTRACING_INSTANCE_DESC d3d12InstanceDesc
        {
            .InstanceID = 0,
            .InstanceMask = 1,
            .InstanceContributionToHitGroupIndex = instance.HitGroupIndex,
            .Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE,
            .AccelerationStructure = instance.Blas.GetBuffer()->GetGpuVirtualAddress(),
        };

        const DirectX::XMMATRIX transposedMatrix = DirectX::XMMatrixTranspose(instance.Transform);
        memcpy(&d3d12InstanceDesc.Transform, &transposedMatrix, sizeof(DirectX::XMFLOAT3X4));

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

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12BuildInputs
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD,
            .NumDescs = (uint32_t)m_D3D12InstanceDescs.size(),
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .InstanceDescs = m_InstanceBuffer->GetGpuVirtualAddress(),
        };

        RayTracing_AcclerationStructure::AllocateBuffers(device, debugName, d3d12BuildInputs);
    }

    void RayTracing_Tlas::AllocateInstanceBuffer(Device& device, std::string_view debugName)
    {
        BenzinAssert(!m_D3D12InstanceDescs.empty());

        MakeUniquePtr(m_InstanceBuffer, device, BufferCreation
        {
            .DebugName = std::format("TLAS_InstanceBuffer_{}", debugName),
            .MemoryType = ResourceMemoryType::Upload,// TODO: Remove UploadBuffer
            .ElementSizeInBytes = sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
            .ElementCount = (uint32_t)m_D3D12InstanceDescs.size(),
        });

        BufferWriter writer{ m_InstanceBuffer->GetCpuMappedData(), m_InstanceBuffer->GetSizeInBytes() };
        writer.WriteData(std::as_bytes(std::span{ m_D3D12InstanceDescs }));
    }

}
