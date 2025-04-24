#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/buffer.hpp"

#include "benzin/core/math.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/d3d12_assert.hpp"

namespace benzin
{

    struct BufferSrv
    {
        BufferType BufferType = BufferType::Byte;
        IndexRange64 ElementRange{};
    };

    struct BufferUav {};

    struct BufferCbv
    {
        uint32_t ElementIndex = 0;
    };

    static void ValidateBufferElementRange(const Buffer& buffer, IndexRange64& outElementRange)
    {
        BenzinAssert(outElementRange.StartIndex < buffer.GetElementCount());
        BenzinAssert(outElementRange.Count <= buffer.GetElementCount());

        outElementRange.Count = outElementRange.Count != 0 ? outElementRange.Count : buffer.GetElementCount();
    }

    static D3D12_HEAP_TYPE ToD3D12HeapType(const Device& device, const ResourceMemoryType& memoryType)
    {
        switch (memoryType)
        {
            case ResourceMemoryType::Default: return D3D12_HEAP_TYPE_DEFAULT;
            case ResourceMemoryType::Readback: return D3D12_HEAP_TYPE_READBACK;
            case ResourceMemoryType::Upload: return device.GetCaps().IsGpuUploadHeapsSupported ? D3D12_HEAP_TYPE_GPU_UPLOAD : D3D12_HEAP_TYPE_UPLOAD;
        }

        std::unreachable();
    }

    static D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const BufferCreation& bufferCreation)
    {
        BenzinAssert(bufferCreation.ElementSizeInBytes != 0);
        BenzinAssert(bufferCreation.ElementCount != 0);

        uint32_t alignedElementSizeInBytes = bufferCreation.ElementSizeInBytes;
        if (bufferCreation.Type == BufferType::Const)
        {
            // Align the 'BufferCreation::ElementSize', not the entire buffer size 'BufferType::Const'
            // This is done so that each element can be used as a separate constant buffer using ConstantBufferView
            alignedElementSizeInBytes = AlignUp(alignedElementSizeInBytes, GraphicsConfig::GetConstBufferAlignmentInBytes());
        }
        else if (bufferCreation.Type == BufferType::Structured)
        {
            // Performance tip: Align structures on sizeof(float4) boundary
            // Ref: https://developer.nvidia.com/content/understanding-structured-buffer-performance

            BenzinWarningIf(
                alignedElementSizeInBytes % GraphicsConfig::GetStructuredBufferAlignmentInBytes() != 0,
                "Buffer '{}' is not properly aligned. BufferElementSize: {}, StructuredBufferAlignment: {}",
                bufferCreation.DebugName,
                alignedElementSizeInBytes,
                GraphicsConfig::GetStructuredBufferAlignmentInBytes()
            );
        }

        D3D12_RESOURCE_FLAGS d3d12ResourceFlags = D3D12_RESOURCE_FLAG_NONE;
        if (bufferCreation.IsUnorderedAccessAllowed)
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        return D3D12_RESOURCE_DESC
        {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = alignedElementSizeInBytes * bufferCreation.ElementCount,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc{ 1, 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = d3d12ResourceFlags,
        };
    }

    static void CreateD3D12Resource(
        const BufferCreation& bufferCreation,
        const Device& device,
        ID3D12Resource*& outD3D12Resource,
        ResourceState& outInitialState
    )
    {
        static const auto getInitialResourceState = [](const Device& device, const BufferCreation& creation)
        {
            if (creation.Type == BufferType::RayTracing_AccelerationStructure)
            {
                return ResourceState::RayTracing_AccelerationStructure;
            }
            else if (creation.MemoryType == ResourceMemoryType::Upload && !device.GetCaps().IsGpuUploadHeapsSupported)
            {
                // Case only for D3D12_HEAP_TYPE_UPLOAD
                // D3D12_HEAP_TYPE_GPU_UPLOAD requires D3D12_RESOURCE_STATE_COMMON
                return ResourceState::GenericRead;
            }
            else if (creation.MemoryType == ResourceMemoryType::Readback)
            {
                return ResourceState::CopyDestination;
            }

            return ResourceState::Common;
        };

        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(ToD3D12HeapType(device, bufferCreation.MemoryType));
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(bufferCreation);

        outInitialState = getInitialResourceState(device, bufferCreation);

        BenzinD3D12Call(device.GetD3D12Device()->CreateCommittedResource(
            &d3d12HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &d3d12ResourceDesc,
            (D3D12_RESOURCE_STATES)outInitialState,
            nullptr,
            IID_PPV_ARGS(&outD3D12Resource)
        ));

        BenzinEnsure(outD3D12Resource != nullptr);
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResoureViewDesc(const Buffer& buffer, IndexRange64 elementRange)
    {
        switch (buffer.GetType())
        {
            case BufferType::Byte:
            {
                // Note: ByteAddressBuffers supports only 'DXGI_FORMAT_R32_TYPELESS' format 
                // Ref: https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-intro#raw-views-of-buffers

                static const auto rawBufferFormat = GraphicsFormat::R32Typeless;
                static const auto rawBufferFormatSizeInBytes = GetFormatSizeInBytes(rawBufferFormat);

                BenzinAssert(buffer.GetSizeInBytes() % rawBufferFormatSizeInBytes == 0);

                return D3D12_SHADER_RESOURCE_VIEW_DESC
                {
                    .Format = (DXGI_FORMAT)rawBufferFormat,
                    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .Buffer
                    {
                        .FirstElement = 0,
                        .NumElements = (uint32_t)(buffer.GetSizeInBytes() / rawBufferFormatSizeInBytes),
                        .StructureByteStride = 0,
                        .Flags = D3D12_BUFFER_SRV_FLAG_RAW,
                    },
                };
            }
            case BufferType::Format:
            {
                return D3D12_SHADER_RESOURCE_VIEW_DESC
                {
                    .Format = (DXGI_FORMAT)buffer.GetFormat(),
                    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .Buffer
                    {
                        .FirstElement = elementRange.StartIndex,
                        .NumElements = (uint32_t)elementRange.Count,
                        .StructureByteStride = 0,
                        .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
                    },
                };
            }
            case BufferType::Structured:
            {
                // Ref: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_buffer_srv#remarks

                return D3D12_SHADER_RESOURCE_VIEW_DESC
                {
                    .Format = DXGI_FORMAT_UNKNOWN,
                    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .Buffer
                    {
                        .FirstElement = elementRange.StartIndex,
                        .NumElements = (uint32_t)elementRange.Count,
                        .StructureByteStride = buffer.GetAlignedElementSizeInBytes(), // #TODO: 'm_AlignedElementSize' when using 'StructuredBuffer'?
                        .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
                    },
                };
            }
            case BufferType::RayTracing_AccelerationStructure:
            {
                return D3D12_SHADER_RESOURCE_VIEW_DESC
                {
                    .Format = DXGI_FORMAT_UNKNOWN,
                    .ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .RaytracingAccelerationStructure
                    {
                        .Location = buffer.GetGpuVirtualAddress(),
                    },
                };
            }
        }

        BenzinEnsure(false, "Unknown or unhandled BufferType: {} ({})", magic_enum::enum_name(buffer.GetType()), magic_enum::enum_integer(buffer.GetType()));
        std::unreachable();
    }

    static D3D12_UNORDERED_ACCESS_VIEW_DESC ToD3D12UnorderedAccessViewDesc(const Buffer& buffer)
    {
        switch (buffer.GetType())
        {
            case BufferType::Format:
            {
                BenzinAssert(buffer.GetFormat() != GraphicsFormat::Unknown);

                return D3D12_UNORDERED_ACCESS_VIEW_DESC
                {
                    .Format = (DXGI_FORMAT)buffer.GetFormat(),
                    .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                    .Buffer
                    {
                        .FirstElement = 0,
                        .NumElements = (uint32_t)buffer.GetElementCount(),
                        .StructureByteStride = 0,
                        .CounterOffsetInBytes = 0,
                        .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
                    },
                };
            }
            case BufferType::Structured:
            {
                return D3D12_UNORDERED_ACCESS_VIEW_DESC
                {
                    .Format = DXGI_FORMAT_UNKNOWN,
                    .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                    .Buffer
                    {
                        .FirstElement = 0,
                        .NumElements = (uint32_t)buffer.GetElementCount(),
                        .StructureByteStride = buffer.GetElementSizeInBytes(),
                        .CounterOffsetInBytes = 0,
                        .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
                    },
                };
            }
            default:
            {
                BenzinAssert(false);
            }
        }

        std::unreachable();
    }

    static D3D12_CONSTANT_BUFFER_VIEW_DESC ToD3D12ConstantBufferViewDesc(const Buffer& buffer, uint32_t elementIndex)
    {
        return D3D12_CONSTANT_BUFFER_VIEW_DESC
        {
            .BufferLocation = buffer.GetGpuVirtualAddress(elementIndex),
            .SizeInBytes = buffer.GetAlignedElementSizeInBytes(),
        };
    }

    //


    Buffer::Buffer(Device& device, const BufferCreation& creation)
        : Resource{ device }
    {
        BenzinAssert(m_D3D12Resource == nullptr);

        CreateD3D12Resource(creation, m_Device, m_D3D12Resource, m_CurrentState);
        SetD3DObjectDebugName(m_D3D12Resource, creation.DebugName);

        m_MemoryType = creation.MemoryType;
        m_Type = creation.Type;
        m_Format = creation.Format;

        m_ElementSizeInBytes = creation.ElementSizeInBytes;
        m_ElementCount = creation.ElementCount;
        m_AlignedElementSizeInBytes = (uint32_t)(m_D3D12Resource->GetDesc().Width / creation.ElementCount); // HACK

        m_IsUnorderedAccessAllowed = creation.IsUnorderedAccessAllowed;

        if (creation.MemoryType == ResourceMemoryType::Upload)
        {
            const D3D12_RANGE d3d12Range{ .Begin = 0, .End = 0 }; // Writing only range
            BenzinD3D12Call(m_D3D12Resource->Map(0, &d3d12Range, reinterpret_cast<void**>(&m_CpuMappedData)));
        }
    }

    Buffer::~Buffer()
    {
        if (m_CpuMappedData != nullptr)
        {
            m_D3D12Resource->Unmap(0, nullptr);
        }
    }

    uint64_t Buffer::GetGpuVirtualAddress(uint32_t elementIndex) const
    {
        BenzinAssert(m_D3D12Resource != nullptr);
        BenzinAssert(elementIndex < m_ElementCount);

        return m_D3D12Resource->GetGPUVirtualAddress() + elementIndex * m_AlignedElementSizeInBytes;
    }

    const Descriptor& Buffer::GetSrv(IndexRange64 elementRange) const
    {
        ValidateBufferElementRange(*this, elementRange);

        return TryGetViewDescriptor(
            GetStdHash(BufferSrv{ m_Type, elementRange }),
            [&] { return CreateDetachedSrv(elementRange, false); }
        );
    }

    const Descriptor& Buffer::GetUav() const
    {
        return TryGetViewDescriptor(
            GetStdHash(BufferUav{}),
            [&] { return CreateDetachedUav(); }
        );
    }

    const Descriptor& Buffer::GetCbv(uint32_t elementIndex) const
    {
        return TryGetViewDescriptor(
            GetStdHash(BufferCbv{ elementIndex }),
            [&] { return CreateDetachedCbv(elementIndex); }
        );
    }

    Descriptor Buffer::CreateDetachedSrv(IndexRange64 elementRange, bool isValidationEnabled) const
    {
        if (isValidationEnabled)
        {
            ValidateBufferElementRange(*this, elementRange);
        }

        ID3D12Resource* d3d12Resource = nullptr;
        if (m_Type != BufferType::RayTracing_AccelerationStructure)
        {
            BenzinAssert(m_D3D12Resource != nullptr);
            d3d12Resource = m_D3D12Resource;
        }

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Srv, [&](uint64_t cpuHandle)
        {
            const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = ToD3D12ShaderResoureViewDesc(*this, elementRange);

            m_Device.GetD3D12Device()->CreateShaderResourceView(
                d3d12Resource,
                &d3d12SrvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle }
            );
        });
    }

    Descriptor Buffer::CreateDetachedUav() const
    {
        BenzinAssert(m_D3D12Resource != nullptr);
        BenzinAssert(m_IsUnorderedAccessAllowed);

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Uav, [&](uint64_t cpuHandle)
        {
            const D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc = ToD3D12UnorderedAccessViewDesc(*this);

            m_Device.GetD3D12Device()->CreateUnorderedAccessView(
                m_D3D12Resource,
                nullptr,
                &d3d12UavDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle }
            );
        });
    }

    Descriptor Buffer::CreateDetachedCbv(uint32_t elementIndex) const
    {
        BenzinAssert(m_D3D12Resource != nullptr);
        BenzinAssert(m_Type == BufferType::Const);
        BenzinAssert(elementIndex < m_ElementCount);

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Cbv, [&](uint64_t cpuHandle)
        {
            const D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12CbvDesc = ToD3D12ConstantBufferViewDesc(*this, elementIndex);

            m_Device.GetD3D12Device()->CreateConstantBufferView(
                &d3d12CbvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle }
            );
        });
    }

    void Buffer::MapReadbackData(uint64_t offsetInBytes, uint32_t dataSizeInBytes, const MapReadbackCallback& callback) const
    {
        BenzinAssert(m_MemoryType == ResourceMemoryType::Readback);
        BenzinAssert(offsetInBytes + dataSizeInBytes <= GetSizeInBytes());
        BenzinAssert(callback);

        const D3D12_RANGE d3d12ReadbackRange
        {
            .Begin = offsetInBytes,
            .End = offsetInBytes + dataSizeInBytes,
        };

        std::byte* mappedData = nullptr;
        BenzinD3D12Call(m_D3D12Resource->Map(0, &d3d12ReadbackRange, reinterpret_cast<void**>(&mappedData)));

        callback(mappedData);

        m_D3D12Resource->Unmap(0, nullptr);
    }

}

BenzinDefineStdHashForType(benzin::BufferSrv, bufferSrv,
{
    size_t hash = typeid(benzin::BufferSrv).hash_code();
    hash = benzin::HashCombine(hash, bufferSrv.BufferType);
    hash = benzin::HashCombine(hash, bufferSrv.ElementRange.StartIndex);
    hash = benzin::HashCombine(hash, bufferSrv.ElementRange.Count);

    return hash;
});

BenzinDefineStdHashForType(benzin::BufferUav, bufferUav,
{
    return typeid(benzin::BufferUav).hash_code();
});

BenzinDefineStdHashForType(benzin::BufferCbv, bufferCbv,
{
    size_t hash = typeid(benzin::BufferCbv).hash_code();
    hash = benzin::HashCombine(hash, bufferCbv.ElementIndex);

    return hash;
});
