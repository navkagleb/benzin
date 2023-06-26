#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/buffer.hpp"

#include "benzin/core/common.hpp"

namespace benzin
{

    namespace
    {

        void ValidateBufferResourceConfig(BufferResource::Config& config)
        {
            using namespace magic_enum::bitwise_operators;
            using Flags = BufferResource::Flags;

            if ((config.Flags & Flags::ConstantBuffer) != Flags::None)
            {
                config.ElementSize = AlignAbove<uint32_t>(config.ElementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            }
        }

        D3D12_HEAP_PROPERTIES GetD3D12HeapProperties(D3D12_HEAP_TYPE d3d12HeapType)
        {
            return D3D12_HEAP_PROPERTIES
            {
                .Type{ d3d12HeapType },
                .CPUPageProperty{ D3D12_CPU_PAGE_PROPERTY_UNKNOWN },
                .MemoryPoolPreference{ D3D12_MEMORY_POOL_UNKNOWN },
                .CreationNodeMask{ 1 },
                .VisibleNodeMask{ 1 },
            };
        }

        D3D12_RESOURCE_DESC ConvertToD3D12ResourceDesc(const BufferResource::Config& config)
        {
            return D3D12_RESOURCE_DESC
            {
                .Dimension{ D3D12_RESOURCE_DIMENSION_BUFFER },
                .Alignment{ 0 },
                .Width{ static_cast<uint64_t>(config.ElementSize * config.ElementCount) },
                .Height{ 1 },
                .DepthOrArraySize{ 1 },
                .MipLevels{ 1 },
                .Format{ DXGI_FORMAT_UNKNOWN },
                .SampleDesc{ 1, 0 },
                .Layout{ D3D12_TEXTURE_LAYOUT_ROW_MAJOR },
                .Flags{ D3D12_RESOURCE_FLAG_NONE }
            };
        }

    } // anonymous namespace

    BufferResource::BufferResource(Device& device, const Config& config)
        : Resource{ device }
        , m_Config{ config }
    {
        BENZIN_ASSERT(device.GetD3D12Device());

        ValidateBufferResourceConfig(m_Config);

        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(config.Flags == Flags::None ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_HEAP_FLAGS d3d12HeapFlags = D3D12_HEAP_FLAG_NONE;

        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ConvertToD3D12ResourceDesc(m_Config);

        if (d3d12HeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD)
        {
            m_CurrentState = State::GenericRead;
        }

        const D3D12_CLEAR_VALUE* d3d12OptimizedClearValue = nullptr;
        BENZIN_HR_ASSERT(m_Device.GetD3D12Device()->CreateCommittedResource(
            &d3d12HeapProperties,
            d3d12HeapFlags,
            &d3d12ResourceDesc,
            static_cast<D3D12_RESOURCE_STATES>(m_CurrentState),
            d3d12OptimizedClearValue,
            IID_PPV_ARGS(&m_D3D12Resource)
        ));
    }

    bool BufferResource::HasConstantBufferView(uint32_t index) const
    {
        return HasResourceView(Descriptor::Type::ConstantBufferView, index);
    }

    const Descriptor& BufferResource::GetConstantBufferView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::ConstantBufferView, index);
    }

    uint32_t BufferResource::PushShaderResourceView()
    {
        BENZIN_ASSERT(m_D3D12Resource);

        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SRVDesc
        {
            .Format{ DXGI_FORMAT_UNKNOWN },
            .ViewDimension{ D3D12_SRV_DIMENSION_BUFFER },
            .Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
            .Buffer
            {
                .FirstElement{ 0 },
                .NumElements{ m_Config.ElementCount },
                .StructureByteStride{ m_Config.ElementSize },
                .Flags{ D3D12_BUFFER_SRV_FLAG_NONE },
            }
        };

        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::ShaderResourceView);

        m_Device.GetD3D12Device()->CreateShaderResourceView(
            m_D3D12Resource,
            &d3d12SRVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(Descriptor::Type::ShaderResourceView, descriptor);
    }

    uint32_t BufferResource::PushConstantBufferView()
    {
        BENZIN_ASSERT(m_D3D12Resource);

        const D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12CBVDesc
        {
            .BufferLocation{ GetGPUVirtualAddress() },
            .SizeInBytes{ m_Config.ElementCount * m_Config.ElementSize }
        };

        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::ShaderResourceView);

        m_Device.GetD3D12Device()->CreateConstantBufferView(
            &d3d12CBVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(Descriptor::Type::ConstantBufferView, descriptor);
    }

} // namespace benzin
