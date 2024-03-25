#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/buffer.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"

namespace benzin
{

    struct FormatBufferSrv
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
    };

    struct StructuredBufferSrv
    {
        IndexRangeU32 ElementRange;
    };

    struct ByteAddressBufferSrv {};
    struct RtAsBufferSrv {};

    struct BufferUav {};

    struct BufferCbv
    {
        uint32_t ElementIndex = 0;
    };

    static D3D12_HEAP_TYPE ResolveD3D12HeapType(const Device& device, BufferFlags flags)
    {
        if (flags.IsAnySet(BufferFlag::UploadBuffer | BufferFlag::ConstantBuffer))
        {
            return device.IsGpuUploadHeapsSupported() ? D3D12_HEAP_TYPE_GPU_UPLOAD : D3D12_HEAP_TYPE_UPLOAD;
        }
        else if (flags.IsSet(BufferFlag::ReadbackBuffer))
        {
            return D3D12_HEAP_TYPE_READBACK;
        }
            
        return D3D12_HEAP_TYPE_DEFAULT;
    }

    static D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const BufferCreation& bufferCreation)
    {
        BenzinAssert(bufferCreation.ElementSize != 0);
        BenzinAssert(bufferCreation.ElementCount != 0);

        uint32_t alignedElementSize = bufferCreation.ElementSize;
        if (bufferCreation.Flags.IsSet(BufferFlag::ConstantBuffer))
        {
            // The 'BufferCreation::ElementSize' is aligned, not the entire buffer size 'BufferFlag::ConstantBuffer'.
            // This is done so that each element can be used as a separate constant buffer using ConstantBufferView
            alignedElementSize = AlignAbove(alignedElementSize, config::g_ConstantBufferAlignment);
        }
        else if (bufferCreation.Flags.IsSet(BufferFlag::StructuredBuffer))
        {
            // Performance tip: Align structures on sizeof(float4) boundary
            // Ref: https://developer.nvidia.com/content/understanding-structured-buffer-performance

            BenzinAssert(!bufferCreation.DebugName.empty()); // #TODO: Remove. Rewrite warning logging
            BenzinWarningIf(
                alignedElementSize % config::g_StructuredBufferAlignment != 0,
                "For better performance 'ElementSize' ({}) should be aligned to StructuredBufferAlignemnt ({}) in buffer: {}",
                alignedElementSize,
                config::g_StructuredBufferAlignment,
                bufferCreation.DebugName
            );
        }

        D3D12_RESOURCE_FLAGS d3d12ResourceFlags = D3D12_RESOURCE_FLAG_NONE;
        if (bufferCreation.Flags.IsSet(BufferFlag::AllowUnorderedAccess))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        return D3D12_RESOURCE_DESC
        {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = alignedElementSize * bufferCreation.ElementCount,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc{ 1, 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = d3d12ResourceFlags,
        };
    }

    static void CreateD3D12Resource(const BufferCreation& bufferCreation, const Device& device, ID3D12Resource*& d3d12Resource)
    {
        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(ResolveD3D12HeapType(device, bufferCreation.Flags));
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(bufferCreation);

        if ((d3d12HeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD || d3d12HeapProperties.Type == D3D12_HEAP_TYPE_GPU_UPLOAD) && bufferCreation.InitialState != ResourceState::GenericRead)
        {
            // Validate 'InitialState'
            const_cast<BufferCreation&>(bufferCreation).InitialState = ResourceState::GenericRead;
        }

        BenzinEnsure(device.GetD3D12Device()->CreateCommittedResource(
            &d3d12HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &d3d12ResourceDesc,
            static_cast<D3D12_RESOURCE_STATES>(bufferCreation.InitialState),
            nullptr,
            IID_PPV_ARGS(&d3d12Resource)
        ));

        BenzinEnsure(d3d12Resource);
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResoureViewDesc(const Buffer& buffer, const FormatBufferSrv& formatSrv)
    {
        return D3D12_SHADER_RESOURCE_VIEW_DESC
        {
            .Format = (DXGI_FORMAT)formatSrv.Format,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer
            {
                .FirstElement = 0,
                .NumElements = buffer.GetElementCount(),
                .StructureByteStride = 0,
                .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
            },
        };
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResoureViewDesc(const Buffer& buffer, const StructuredBufferSrv& structuredSrv)
    {
        // Ref: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_buffer_srv#remarks

        return D3D12_SHADER_RESOURCE_VIEW_DESC
        {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer
            {
                .FirstElement = structuredSrv.ElementRange.StartIndex,
                .NumElements = structuredSrv.ElementRange.Count,
                .StructureByteStride = buffer.GetAlignedElementSize(), // #TODO: 'm_AlignedElementSize' when using 'StructuredBuffer'?
                .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
            },
        };
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResoureViewDesc(const Buffer& buffer, ByteAddressBufferSrv)
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
                .NumElements = buffer.GetSizeInBytes() / rawBufferFormatSizeInBytes,
                .StructureByteStride = 0,
                .Flags = D3D12_BUFFER_SRV_FLAG_RAW,
            },
        };
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResoureViewDesc(const Buffer& buffer, RtAsBufferSrv)
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

    //

    Buffer::Buffer(Device& device)
        : Resource{ device }
    {}

    Buffer::Buffer(Device& device, const BufferCreation& creation)
        : Buffer{ device }
    {
        Create(creation);
    }

    Buffer::~Buffer()
    {
        if (m_MappedData != nullptr)
        {
            m_D3D12Resource->Unmap(0, nullptr);
        }
    }

    uint64_t Buffer::GetGpuVirtualAddress(uint32_t elementIndex) const
    {
        BenzinAssert(m_D3D12Resource);
        BenzinAssert(elementIndex < m_ElementCount);

        return m_D3D12Resource->GetGPUVirtualAddress() + elementIndex * m_AlignedElementSize;
    }

    void Buffer::Create(const BufferCreation& creation)
    {
        BenzinAssert(!m_D3D12Resource);

        CreateD3D12Resource(creation, m_Device, m_D3D12Resource);
        SetD3D12ObjectDebugName(m_D3D12Resource, creation.DebugName);

        m_CurrentState = creation.InitialState; // 'InitialState' can updated in 'CreateD3D12Resource'
        m_ElementSize = creation.ElementSize;
        m_ElementCount = creation.ElementCount;
        m_AlignedElementSize = (uint32_t)m_D3D12Resource->GetDesc().Width / creation.ElementCount; // HACK

        if (creation.Flags.IsAnySet(BufferFlag::UploadBuffer | BufferFlag::ConstantBuffer))
        {
            const D3D12_RANGE d3d12Range{ .Begin = 0, .End = 0 }; // Writing only range
            BenzinAssert(m_D3D12Resource->Map(0, &d3d12Range, reinterpret_cast<void**>(&m_MappedData)));
        }

        if (!creation.InitialData.empty())
        {
            BenzinAssert(creation.Flags.IsAnySet(BufferFlag::UploadBuffer | BufferFlag::ConstantBuffer));

            const MemoryWriter writer{ GetMappedData(), GetSizeInBytes() };
            writer.WriteBytes(creation.InitialData);
        }
    }

    const Descriptor& Buffer::GetFormatSrv(GraphicsFormat format) const
    {
        BenzinAssert(format != GraphicsFormat::Unknown);
        BenzinAssert(m_ElementSize == GetFormatSizeInBytes(format));

        const FormatBufferSrv formatSrv{ format };

        auto& descriptor = GetViewDescriptor(formatSrv);
        if (!descriptor.IsCpuValid())
        {
            const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = ToD3D12ShaderResoureViewDesc(*this, formatSrv);
            descriptor = CreateSrv(d3d12SrvDesc, m_D3D12Resource);
        }

        return descriptor;
    }

    const Descriptor& Buffer::GetStructuredSrv(IndexRangeU32 elementRange) const
    {
        BenzinAssert(elementRange.StartIndex < m_ElementCount);

        elementRange.Count = elementRange.Count != 0 ? elementRange.Count : m_ElementCount;
        const StructuredBufferSrv structuredSrv{ elementRange };

        auto& descriptor = GetViewDescriptor(structuredSrv);
        if (!descriptor.IsCpuValid())
        {
            const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = ToD3D12ShaderResoureViewDesc(*this, structuredSrv);
            descriptor = CreateSrv(d3d12SrvDesc, m_D3D12Resource);
        }

        return descriptor;
    }

    const Descriptor& Buffer::GetByteAddressSrv() const
    {
        auto& descriptor = GetViewDescriptor(ByteAddressBufferSrv{});
        if (!descriptor.IsCpuValid())
        {
            const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = ToD3D12ShaderResoureViewDesc(*this, ByteAddressBufferSrv{});
            descriptor = CreateSrv(d3d12SrvDesc, m_D3D12Resource);
        }

        return descriptor;
    }

    const Descriptor& Buffer::GetRtAsSrv() const
    {
        auto& descriptor = GetViewDescriptor(RtAsBufferSrv{});
        if (!descriptor.IsCpuValid())
        {
            const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = ToD3D12ShaderResoureViewDesc(*this, RtAsBufferSrv{});
            descriptor = CreateSrv(d3d12SrvDesc, nullptr);
        }

        return descriptor;
    }

    const Descriptor& Buffer::GetUav() const
    {
        BenzinAssert(m_D3D12Resource);

        auto& descriptor = GetViewDescriptor(BufferUav{});
        if (!descriptor.IsCpuValid())
        {
            descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Uav);

            const D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc
            {
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                .Buffer
                {
                    .FirstElement = 0,
                    .NumElements = m_ElementCount,
                    .StructureByteStride = m_ElementSize,
                    .CounterOffsetInBytes = 0,
                    .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
                }
            };

            m_Device.GetD3D12Device()->CreateUnorderedAccessView(
                m_D3D12Resource,
                nullptr,
                &d3d12UavDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
            );
        }

        return descriptor;
    }

    const Descriptor& Buffer::GetCbv(uint32_t elementIndex) const
    {
        BenzinAssert(m_D3D12Resource);
        BenzinAssert(elementIndex < m_ElementCount);

        auto& descriptor = GetViewDescriptor(BufferCbv{ elementIndex });
        if (!descriptor.IsCpuValid())
        {
            descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Cbv);

            const D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12CbvDesc
            {
                .BufferLocation = GetGpuVirtualAddress(elementIndex),
                .SizeInBytes = m_AlignedElementSize,
            };

            m_Device.GetD3D12Device()->CreateConstantBufferView(
                &d3d12CbvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
            );
        }

        return descriptor;
    }

    Descriptor Buffer::CreateSrv(const D3D12_SHADER_RESOURCE_VIEW_DESC& d3d12SrvDesc, ID3D12Resource* d3d12Resource) const
    {
        Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Srv);

        m_Device.GetD3D12Device()->CreateShaderResourceView(
            d3d12Resource,
            &d3d12SrvDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
        );

        return descriptor;
    }

} // namespace benzin

template <>
struct std::hash<benzin::FormatBufferSrv>
{
    size_t operator()(const benzin::FormatBufferSrv& formatSrv) const
    {
        static const auto baseHash = typeid(benzin::FormatBufferSrv).hash_code();

        size_t hash = baseHash;
        hash = benzin::HashCombine(hash, formatSrv.Format);

        return hash;
    }
};

template <>
struct std::hash<benzin::StructuredBufferSrv>
{
    size_t operator()(const benzin::StructuredBufferSrv& structuredSrv) const
    {
        static const auto baseHash = typeid(benzin::StructuredBufferSrv).hash_code();

        size_t hash = baseHash;
        hash = benzin::HashCombine(hash, structuredSrv.ElementRange.StartIndex);
        hash = benzin::HashCombine(hash, structuredSrv.ElementRange.Count);

        return hash;
    }
};

template <>
struct std::hash<benzin::ByteAddressBufferSrv>
{
    size_t operator()(benzin::ByteAddressBufferSrv) const
    {
        static const auto baseHash = typeid(benzin::ByteAddressBufferSrv).hash_code();
        return baseHash;
    }
};

template <>
struct std::hash<benzin::RtAsBufferSrv>
{
    size_t operator()(benzin::RtAsBufferSrv) const
    {
        static const auto baseHash = typeid(benzin::RtAsBufferSrv).hash_code();
        return baseHash;
    }
};

template <>
struct std::hash<benzin::BufferUav>
{
    size_t operator()(benzin::BufferUav) const
    {
        static const auto baseHash = typeid(benzin::BufferUav).hash_code();
        return baseHash;
    }
};

template <>
struct std::hash<benzin::BufferCbv>
{
    size_t operator()(const benzin::BufferCbv& bufferCbv) const
    {
        static const auto baseHash = typeid(benzin::BufferCbv).hash_code();

        size_t hash = baseHash;
        hash = benzin::HashCombine(hash, bufferCbv.ElementIndex);

        return hash;
    }
};
