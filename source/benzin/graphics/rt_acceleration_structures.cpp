#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/rt_acceleration_structures.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin::rt
{

    namespace
    {

        D3D12_RAYTRACING_GEOMETRY_DESC ToD3D12RaytracingGeometryDesc(const TriangledGeometry& geometry)
        {
            const uint32_t validatedVertexCount = geometry.VertexCount == g_InvalidIndex<uint32_t> && geometry.VertexOffset == 0 ? geometry.VertexBuffer.GetElementCount() : geometry.VertexCount;
            const uint32_t validatedIndexCount = geometry.IndexCount == g_InvalidIndex<uint32_t> && geometry.IndexOffset == 0 ? geometry.IndexBuffer.GetElementCount() : geometry.IndexCount;

            return D3D12_RAYTRACING_GEOMETRY_DESC
            {
                .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
                .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
                .Triangles
                {
                    .Transform3x4 = 0,
                    .IndexFormat = DXGI_FORMAT_R32_UINT,
                    .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                    .IndexCount = validatedIndexCount,
                    .VertexCount = validatedVertexCount,
                    .IndexBuffer = geometry.IndexBuffer.GetGPUVirtualAddress(geometry.IndexOffset),
                    .VertexBuffer
                    {
                        .StartAddress = geometry.VertexBuffer.GetGPUVirtualAddress(geometry.VertexOffset),
                        .StrideInBytes = geometry.VertexBuffer.GetElementSize(),
                    },
                },
            };
        }

        D3D12_RAYTRACING_GEOMETRY_DESC ToD3D12RaytracingGeometryDesc(const ProceduralGeometry& geometry)
        {
            BenzinAssert(geometry.AABBBuffer.GetElementSize() == sizeof(D3D12_RAYTRACING_AABB));

            return D3D12_RAYTRACING_GEOMETRY_DESC
            {
                .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS,
                .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
                .AABBs
                {
                    .AABBCount = 1,
                    .AABBs
                    {
                        .StartAddress = geometry.AABBBuffer.GetGPUVirtualAddress(geometry.AABBIndex),
                        .StrideInBytes = geometry.AABBBuffer.GetElementSize(),
                    },
                },
            };
        }

        D3D12_RAYTRACING_GEOMETRY_DESC ToD3D12RaytracingGeometryDescVariant(const GeometryVariant& geometryVariant)
        {
            return geometryVariant | VisitorMatch
            {
                [](auto&& geometry) { return ToD3D12RaytracingGeometryDesc(geometry); },
            };
        }

        D3D12_RAYTRACING_INSTANCE_DESC ToD3D12RaytracingInstanceDesc(const TopLevelInstance& instance)
        {
            // D3D12_RAYTRACING_INSTANCE_DESC::InstanceID - 24 bit
            // D3D12_RAYTRACING_INSTANCE_DESC::InstanceMask - 8 bit - Bitwise AND with TraceRay() parameter
            // D3D12_RAYTRACING_INSTANCE_DESC::InstanceContributionToHitGroupIndex - 24 bit - Chose hit group shader
            // D3D12_RAYTRACING_INSTANCE_DESC::Flags - 8 bit

            D3D12_RAYTRACING_INSTANCE_DESC d3d12InstanceDesc
            {
                .InstanceID = 0,
                .InstanceMask = 1,
                .InstanceContributionToHitGroupIndex = instance.HitGroupIndex, 
                .Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE,
                .AccelerationStructure = instance.BottomLevelAccelerationStructure.GetBuffer().GetGPUVirtualAddress(),
            };

            const DirectX::XMMATRIX transposedMatrix = DirectX::XMMatrixTranspose(instance.Transform);
            memcpy(&d3d12InstanceDesc.Transform, &transposedMatrix, sizeof(DirectX::XMFLOAT3X4));

            return d3d12InstanceDesc;
        }

    } // anonymous namespace

    // AccelerationStructure

    AccelerationStructure::AccelerationStructure(Device& device)
        : m_Buffer{ device }
        , m_ScratchResource{ device }
    {}

    void AccelerationStructure::Create(const AccelerationStructureCreation& creation)
    {
        m_D3D12BuildInputs = creation.D3D12BuildInputs;

        const bool isTopLevelAS = m_D3D12BuildInputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        const std::string_view asTypeName = isTopLevelAS ? "TopLevel" : "BottomLevel";

        m_Buffer.Create(BufferCreation
        {
            .ElementCount = (uint32_t)creation.Sizes.BufferSizeInBytes,
            .Flags = BufferFlag::AllowUnorderedAccess,
            .InitialState = ResourceState::RaytracingAccelerationStructure,
            .IsNeedRaytracingAccelerationStructureView = isTopLevelAS,
        });

        m_ScratchResource.Create(BufferCreation
        {
            .ElementCount = (uint32_t)creation.Sizes.ScratchResourceSizeInBytes,
            .Flags = BufferFlag::AllowUnorderedAccess,
        });

        if (!creation.DebugName.empty())
        {
            SetD3D12ObjectDebugName(m_Buffer.GetD3D12Resource(), std::format("{}_{}AccelerationStructure", creation.DebugName, asTypeName));
            SetD3D12ObjectDebugName(m_ScratchResource.GetD3D12Resource(), std::format("{}_{}ScratchResource", creation.DebugName, asTypeName));
        }
    }

    // BottomLevelAccelerationStructure

    BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(Device& device, const BottomLevelAccelerationStructureCreation& creation)
        : AccelerationStructure{ device }
        , m_D3D12GeometryDescs{ std::from_range, creation.Geometries | std::views::transform(ToD3D12RaytracingGeometryDescVariant) }
    {
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12BuildInputs
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
            // .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
            .NumDescs = (uint32_t)m_D3D12GeometryDescs.size(),
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .pGeometryDescs = m_D3D12GeometryDescs.data(),
        };

        Create(AccelerationStructureCreation
        {
            .DebugName = creation.DebugName,
            .Sizes = device.GetAccelerationStructureSizes(d3d12BuildInputs),
            .D3D12BuildInputs = d3d12BuildInputs,
        });
    }

    // TopLevelAccelerationStructure

    TopLevelAccelerationStructure::TopLevelAccelerationStructure(Device& device, const TopLevelAccelerationStructureCreation& creation)
        : AccelerationStructure{ device }
        , m_D3D12InstanceDescs{ std::from_range, creation.Instances | std::views::transform(ToD3D12RaytracingInstanceDesc) }
        , m_InstanceBuffer{ device }
    {
        m_InstanceBuffer.Create(BufferCreation
        {
            .ElementSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
            .ElementCount = (uint32_t)m_D3D12InstanceDescs.size(),
            .Flags = BufferFlag::UploadBuffer, // #TODO: Remove UploadBuffer
            .InitialData = std::as_bytes(std::span{ m_D3D12InstanceDescs }),
        });

        if (!creation.DebugName.empty())
        {
            SetD3D12ObjectDebugName(m_InstanceBuffer.GetD3D12Resource(), std::format("{}_InstanceBuffer", creation.DebugName));
        }

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12BuildInputs
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD,
            .NumDescs = (uint32_t)m_D3D12InstanceDescs.size(),
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .InstanceDescs = m_InstanceBuffer.GetGPUVirtualAddress(),
        };

        Create(AccelerationStructureCreation
        {
            .DebugName = creation.DebugName,
            .Sizes = device.GetAccelerationStructureSizes(d3d12BuildInputs),
            .D3D12BuildInputs = d3d12BuildInputs,
        });
    }

} // namespace benzin::rt
