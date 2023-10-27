#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/buffer.hpp"

#include "benzin/graphics/command_queue.hpp"

namespace benzin
{

    namespace
    {

        D3D12_HEAP_TYPE ResolveD3D12HeapType(BufferFlags flags)
        {
            if (flags[BufferFlag::UploadBuffer] || flags[BufferFlag::ConstantBuffer])
            {
                return D3D12_HEAP_TYPE_UPLOAD;
            }
            
            return D3D12_HEAP_TYPE_DEFAULT;
        }

        D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const BufferCreation& bufferCreation)
        {
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
                alignedElementSize = AlignAbove(alignedElementSize, config::g_StructuredBufferAlignment);
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

        void CreateD3D12Resource(const BufferCreation& bufferCreation, ID3D12Device* d3d12Device, ID3D12Resource*& d3d12Resource)
        {
            BenzinAssert(d3d12Device);

            const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(ResolveD3D12HeapType(bufferCreation.Flags));
            const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(bufferCreation);

            if (d3d12HeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD && bufferCreation.InitialState != ResourceState::GenericRead)
            {
                // Validate 'InitialState'
                const_cast<BufferCreation&>(bufferCreation).InitialState = ResourceState::GenericRead;
            }

            BenzinAssert(d3d12Device->CreateCommittedResource(
                &d3d12HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &d3d12ResourceDesc,
                static_cast<D3D12_RESOURCE_STATES>(bufferCreation.InitialState),
                nullptr,
                IID_PPV_ARGS(&d3d12Resource)
            ));
        }

    } // anonymous namespace

    Buffer::Buffer(Device& device, const BufferCreation& creation)
        : Resource{ device }
    {
        CreateD3D12Resource(creation, m_Device.GetD3D12Device(), m_D3D12Resource);
        SetD3D12ObjectDebugName(m_D3D12Resource, creation.DebugName);

        m_CurrentState = creation.InitialState; // 'InitialState' can updated in 'CreateD3D12Resource'
        m_ElementSize = creation.ElementSize;
        m_ElementCount = creation.ElementCount;
        m_AlignedElementSize = static_cast<uint32_t>(m_D3D12Resource->GetDesc().Width) / creation.ElementCount; // HACK

        if (!creation.InitialData.empty())
        {
            BenzinAssert(creation.Flags[BufferFlag::UploadBuffer] || creation.Flags[BufferFlag::ConstantBuffer]);

            MappedData buffer{ *this };
            buffer.Write(creation.InitialData);
        }

        if (creation.IsNeedShaderResourceView)
        {
            PushShaderResourceView();
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

    uint32_t Buffer::PushShaderResourceView(const BufferShaderResourceViewCreation& creation)
    {
        BenzinAssert(m_D3D12Resource);

        BenzinAssert(creation.FirstElementIndex < m_ElementCount);

#if BENZIN_IS_ASSERTS_ENABLED
        if (creation.IsByteAddressBuffer)
        {
            BenzinAssert(creation.ElementCount != 0, "When creating 'BufferShaderResourceViewCreation' as ByteAddressBuffer than 'ElementCount' must be specified");
        }
#endif

        BufferShaderResourceViewCreation validatedCreation = creation;
        validatedCreation.ElementCount = creation.ElementCount != 0 ? creation.ElementCount : m_ElementCount;

        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12ShaderResourceViewDesc
        {
            .Format = validatedCreation.IsByteAddressBuffer ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer
            {
                .FirstElement = validatedCreation.FirstElementIndex,
                .NumElements = validatedCreation.ElementCount,
                .StructureByteStride = validatedCreation.IsByteAddressBuffer ? 0 : m_AlignedElementSize, // TODO: 'm_AlignedElementSize' when using 'StructuedBuffer'?
                .Flags = validatedCreation.IsByteAddressBuffer ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE,
            },
        };

        const DescriptorType descriptorType = DescriptorType::ShaderResourceView;
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(descriptorType);

        m_Device.GetD3D12Device()->CreateShaderResourceView(
            m_D3D12Resource,
            &d3d12ShaderResourceViewDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(descriptorType, descriptor);
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
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(descriptorType, descriptor);
    }

    uint32_t Buffer::PushConstantBufferView(const ConstantBufferViewCreation& creation)
    {
        BenzinAssert(m_D3D12Resource);

        uint64_t gpuVirtualAddress = GetGPUVirtualAddress();
        if (creation.ElementIndex != ConstantBufferViewCreation::s_InvalidElementIndex)
        {
            BenzinAssert(creation.ElementIndex < m_ElementCount);
            gpuVirtualAddress += creation.ElementIndex * m_AlignedElementSize;
        }

        const D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12ConstantBufferViewDesc
        {
            .BufferLocation = gpuVirtualAddress,
            .SizeInBytes = m_AlignedElementSize,
        };

        const DescriptorType descriptorType = DescriptorType::ConstantBufferView;
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(descriptorType);

        m_Device.GetD3D12Device()->CreateConstantBufferView(
            &d3d12ConstantBufferViewDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(descriptorType, descriptor);
    }

} // namespace benzin
