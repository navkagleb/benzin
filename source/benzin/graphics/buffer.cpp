#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/buffer.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"

namespace benzin
{

    static D3D12_HEAP_TYPE ResolveD3D12HeapType(const Device& device, BufferFlags flags)
    {
        if (flags[BufferFlag::UploadBuffer] || flags[BufferFlag::ConstantBuffer])
        {
            return device.IsGpuUploadHeapsSupported() ? D3D12_HEAP_TYPE_GPU_UPLOAD : D3D12_HEAP_TYPE_UPLOAD;
        }
        else if (flags[BufferFlag::ReadbackBuffer])
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
        if (bufferCreation.Flags[BufferFlag::ConstantBuffer])
        {
            // The 'BufferCreation::ElementSize' is aligned, not the entire buffer size 'BufferFlag::ConstantBuffer'.
            // This is done so that each element can be used as a separate constant buffer using ConstantBufferView
            alignedElementSize = AlignAbove(alignedElementSize, config::g_ConstantBufferAlignment);
        }
        else if (bufferCreation.Flags[BufferFlag::StructuredBuffer])
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
        if (bufferCreation.Flags[BufferFlag::AllowUnorderedAccess])
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

        if (d3d12HeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD && bufferCreation.InitialState != ResourceState::GenericRead)
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

    static D3D12_SHADER_RESOURCE_VIEW_DESC CreateD3D12FormatBufferView(const Buffer& buffer, const FormatBufferViewCreation& creation)
    {
        BenzinAssert(creation.Format != GraphicsFormat::Unknown);
        BenzinAssert(buffer.GetElementSize() == GetFormatSizeInBytes(creation.Format));

        return D3D12_SHADER_RESOURCE_VIEW_DESC
        {
            .Format = (DXGI_FORMAT)creation.Format,
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

    static D3D12_SHADER_RESOURCE_VIEW_DESC CreateD3D12StructuredBufferView(const Buffer& buffer, const StructuredBufferViewCreation& creation)
    {
        // Ref: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_buffer_srv#remarks

        BenzinAssert(creation.FirstElementIndex < buffer.GetElementCount());

        StructuredBufferViewCreation validatedCreation = creation;
        validatedCreation.ElementCount = creation.ElementCount != 0 ? creation.ElementCount : buffer.GetElementCount();

        return D3D12_SHADER_RESOURCE_VIEW_DESC
        {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer
            {
                .FirstElement = validatedCreation.FirstElementIndex,
                .NumElements = validatedCreation.ElementCount,
                .StructureByteStride = buffer.GetAlignedElementSize(), // #TODO: 'm_AlignedElementSize' when using 'StructuredBuffer'?
                .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
            },
        };
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC CreateD3D12ByteAddressBufferView(const Buffer& buffer)
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

    static D3D12_SHADER_RESOURCE_VIEW_DESC CreateD3D12RaytracingAccelerationStructureView(const Buffer& buffer)
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

    uint64_t Buffer::GetGpuVirtualAddress() const
    {
        BenzinAssert(m_D3D12Resource);
        return m_D3D12Resource->GetGPUVirtualAddress();
    }

    uint64_t Buffer::GetGpuVirtualAddress(uint32_t elementIndex) const
    {
        BenzinAssert(elementIndex < m_ElementCount);
        return GetGpuVirtualAddress() + elementIndex * m_AlignedElementSize;
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

        if (creation.Flags[BufferFlag::UploadBuffer] || creation.Flags[BufferFlag::ConstantBuffer])
        {
            const D3D12_RANGE d3d12Range{ .Begin = 0, .End = 0 }; // Writing only range
            BenzinAssert(m_D3D12Resource->Map(0, &d3d12Range, reinterpret_cast<void**>(&m_MappedData)));
        }

        if (!creation.InitialData.empty())
        {
            BenzinAssert(creation.Flags[BufferFlag::UploadBuffer] || creation.Flags[BufferFlag::ConstantBuffer]);

            const MemoryWriter writer{ GetMappedData(), GetSizeInBytes() };
            writer.WriteBytes(creation.InitialData);
        }

        if (creation.IsNeedFormatBufferView)
        {
            BenzinAssert(creation.Format != GraphicsFormat::Unknown);
            PushFormatBufferView({ .Format = creation.Format });
        }

        if (creation.IsNeedStructuredBufferView)
        {
            PushStructureBufferView();
        }

        if (creation.IsNeedByteAddressBufferView)
        {
            PushByteAddressBufferView();
        }

        if (creation.IsNeedRaytracingAccelerationStructureView)
        {
            PushRaytracingAccelerationStructureView();
        }

        if (creation.IsNeedUnorderedAccessView)
        {
            PushUnorderedAccessView();
        }

        if (creation.IsNeedConstantBufferView)
        {
            PushConstantBufferView();
        }
    }

    uint32_t Buffer::PushFormatBufferView(const FormatBufferViewCreation& creation)
    {
        BenzinAssert(m_D3D12Resource);

        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12ShaderResourceViewDesc = CreateD3D12FormatBufferView(*this, creation);
        return PushShaderResourceView(d3d12ShaderResourceViewDesc, m_D3D12Resource);
    }

    uint32_t Buffer::PushStructureBufferView(const StructuredBufferViewCreation& creation)
    {
        BenzinAssert(m_D3D12Resource);

        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12ShaderResourceViewDesc = CreateD3D12StructuredBufferView(*this, creation);
        return PushShaderResourceView(d3d12ShaderResourceViewDesc, m_D3D12Resource);
    }

    uint32_t Buffer::PushByteAddressBufferView()
    {
        BenzinAssert(m_D3D12Resource);

        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12ShaderResourceViewDesc = CreateD3D12ByteAddressBufferView(*this);
        return PushShaderResourceView(d3d12ShaderResourceViewDesc, m_D3D12Resource);
    }

    uint32_t Buffer::PushRaytracingAccelerationStructureView()
    {
        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12ShaderResourceViewDesc = CreateD3D12RaytracingAccelerationStructureView(*this);
        return PushShaderResourceView(d3d12ShaderResourceViewDesc, nullptr); // RaytracingAccelerationStructureView creates with pResource == nullptr
    }

    uint32_t Buffer::PushUnorderedAccessView()
    {
        BenzinAssert(m_D3D12Resource);

        const D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UnorderedAccessViewDesc
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

        const DescriptorType descriptorType = DescriptorType::UnorderedAccessView;
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(descriptorType);

        m_Device.GetD3D12Device()->CreateUnorderedAccessView(
            m_D3D12Resource,
            nullptr,
            &d3d12UnorderedAccessViewDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
        );

        return PushResourceView(descriptorType, descriptor);
    }

    uint32_t Buffer::PushConstantBufferView(const ConstantBufferViewCreation& creation)
    {
        BenzinAssert(m_D3D12Resource);

        const uint64_t gpuVirtualAddress = IsValidIndex(creation.ElementIndex) ? GetGpuVirtualAddress(creation.ElementIndex) : GetGpuVirtualAddress();

        const D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12ConstantBufferViewDesc
        {
            .BufferLocation = gpuVirtualAddress,
            .SizeInBytes = m_AlignedElementSize,
        };

        const DescriptorType descriptorType = DescriptorType::ConstantBufferView;
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(descriptorType);

        m_Device.GetD3D12Device()->CreateConstantBufferView(
            &d3d12ConstantBufferViewDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
        );

        return PushResourceView(descriptorType, descriptor);
    }

    uint32_t Buffer::PushShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC& d3d12ShaderResourceViewDesc, ID3D12Resource* d3d12Resource)
    {
        const DescriptorType descriptorType = DescriptorType::ShaderResourceView;
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(descriptorType);

        m_Device.GetD3D12Device()->CreateShaderResourceView(
            d3d12Resource,
            &d3d12ShaderResourceViewDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
        );

        return PushResourceView(descriptorType, descriptor);
    }

} // namespace benzin
