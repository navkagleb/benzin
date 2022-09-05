#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/device.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "spieler/core/common.hpp"
#include "spieler/core/assert.hpp"
#include "spieler/core/logger.hpp"

#include "spieler/renderer/buffer.hpp"

#include "platform/dx12/dx12_common.hpp"

namespace spieler::renderer
{

    namespace _internal
    {

        constexpr inline static uint32_t Align(uint32_t value, uint32_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        inline static D3D12_HEAP_PROPERTIES GetDX12HeapProperties(D3D12_HEAP_TYPE type)
        {
            return D3D12_HEAP_PROPERTIES
            {
                .Type{ type },
                .CPUPageProperty{ D3D12_CPU_PAGE_PROPERTY_UNKNOWN },
                .MemoryPoolPreference{ D3D12_MEMORY_POOL_UNKNOWN },
                .CreationNodeMask{ 1 },
                .VisibleNodeMask{ 1 },
            };
        }

        inline static D3D12_RESOURCE_DESC ConvertToDX12ResourceDesc(const BufferResource::Config& config)
        {
            return D3D12_RESOURCE_DESC
            {
                .Dimension{ D3D12_RESOURCE_DIMENSION_BUFFER },
                .Alignment{ config.Alignment },
                .Width{ static_cast<uint64_t>(config.ElementSize) * config.ElementCount },
                .Height{ 1 },
                .DepthOrArraySize{ 1 },
                .MipLevels{ 1 },
                .Format{ DXGI_FORMAT_UNKNOWN },
                .SampleDesc{ 1, 0 },
                .Layout{ D3D12_TEXTURE_LAYOUT_ROW_MAJOR },
                .Flags{ D3D12_RESOURCE_FLAG_NONE }
            };
        }

        inline static D3D12_RESOURCE_DESC ConvertToDX12ResourceDesc(const TextureResource::Config& config)
        {
            return D3D12_RESOURCE_DESC
            {
                .Dimension{ dx12::Convert(config.Dimension) },
                .Alignment{ 0 },
                .Width{ static_cast<uint64_t>(config.Width) },
                .Height{ config.Height },
                .DepthOrArraySize{ config.Depth },
                .MipLevels{ config.MipCount },
                .Format{ dx12::Convert(config.Format) },
                .SampleDesc{ 1, 0 },
                .Layout{ D3D12_TEXTURE_LAYOUT_UNKNOWN },
                .Flags{ dx12::Convert(config.Flags) }
            };
        }

        inline static D3D12_CLEAR_VALUE ConvertToDX12ClearValue(const TextureResource::Config& config, const TextureResource::ClearColor& clearColor)
        {
            return D3D12_CLEAR_VALUE
            {
                .Format{ dx12::Convert(config.Format) },
                .Color 
                { 
                    clearColor.Color.x,
                    clearColor.Color.y,
                    clearColor.Color.z,
                    clearColor.Color.w
                }
            };
        }

        inline static D3D12_CLEAR_VALUE ConvertToDX12ClearValue(const TextureResource::Config& config, const TextureResource::ClearDepthStencil& clearDepthStencil)
        {
            return D3D12_CLEAR_VALUE
            {
                .Format{ dx12::Convert(config.Format) },
                .DepthStencil
                {
                    .Depth{ clearDepthStencil.Depth },
                    .Stencil{ clearDepthStencil.Stencil }
                }
            };
        }

    } // namespace _internal

    constexpr DescriptorManager::Config g_DescriptorManagerConfig
    {
        .CBV_SRV_UAVDescriptorCount{ 100 },
        .SamplerDescriptorCount{ 100 },
        .RTVDescriptorCount{ 100 },
        .DSVDescriptorCount{ 100 }
    };

    Device::Device()
    {
#if defined(SPIELER_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        SPIELER_ASSERT(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))));

        debugController->EnableDebugLayer();
    }
#endif

        // m_Device
        SPIELER_ASSERT(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_DX12Device))));

        // m_DescriptorManager
        m_DescriptorManager = DescriptorManager{ *this, g_DescriptorManagerConfig };
    }

    ComPtr<ID3D12Resource> Device::CreateDX12Buffer(const BufferResource::Config& config)
    {
        SPIELER_ASSERT(config.ElementSize != 0);
        SPIELER_ASSERT(config.ElementCount != 0);

        const D3D12_HEAP_TYPE heapType{ config.Flags == BufferResource::Flags::None ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD };

        const D3D12_HEAP_PROPERTIES heapProperties{ _internal::GetDX12HeapProperties(heapType) };
        const D3D12_RESOURCE_DESC resourceDesc{ _internal::ConvertToDX12ResourceDesc(config) };

        return CreateDX12Resource(&heapProperties, &resourceDesc, nullptr);
    }

    ComPtr<ID3D12Resource> Device::CreateDX12Texture(const TextureResource::Config& config)
    {
        const D3D12_HEAP_PROPERTIES heapProperties{ _internal::GetDX12HeapProperties(D3D12_HEAP_TYPE_DEFAULT) };
        const D3D12_RESOURCE_DESC resourceDesc{ _internal::ConvertToDX12ResourceDesc(config) };

        return CreateDX12Resource(&heapProperties, &resourceDesc, nullptr);
    }

    ComPtr<ID3D12Resource> Device::CreateDX12Texture(const TextureResource::Config& config, const TextureResource::ClearColor& clearColor)
    {
        const D3D12_HEAP_PROPERTIES heapProperties{ _internal::GetDX12HeapProperties(D3D12_HEAP_TYPE_DEFAULT) };
        const D3D12_RESOURCE_DESC resourceDesc{ _internal::ConvertToDX12ResourceDesc(config) };
        const D3D12_CLEAR_VALUE clearValue{ _internal::ConvertToDX12ClearValue(config, clearColor) };

        return CreateDX12Resource(&heapProperties, &resourceDesc, &clearValue);
    }

    ComPtr<ID3D12Resource> Device::CreateDX12Texture(const TextureResource::Config& config, const TextureResource::ClearDepthStencil& clearDepthStencil)
    {
        const D3D12_HEAP_PROPERTIES heapProperties{ _internal::GetDX12HeapProperties(D3D12_HEAP_TYPE_DEFAULT) };
        const D3D12_RESOURCE_DESC resourceDesc{ _internal::ConvertToDX12ResourceDesc(config) };
        const D3D12_CLEAR_VALUE clearValue{ _internal::ConvertToDX12ClearValue(config, clearDepthStencil) };

        return CreateDX12Resource(&heapProperties, &resourceDesc, &clearValue);
    }

    ComPtr<ID3D12Resource> Device::CreateDX12Resource(const D3D12_HEAP_PROPERTIES* heapProperties, const D3D12_RESOURCE_DESC* const resourceDesc, const D3D12_CLEAR_VALUE* const clearValueDesc)
    {
        ComPtr<ID3D12Resource> resource;

        const ::HRESULT result
        {
            m_DX12Device->CreateCommittedResource(
                heapProperties,
                D3D12_HEAP_FLAG_NONE,
                resourceDesc,
                heapProperties->Type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON,
                clearValueDesc,
                IID_PPV_ARGS(&resource)
            )
        };

        if (FAILED(result))
        {
            SPIELER_CRITICAL("Failed to create Resource");
            SPIELER_ASSERT(false);
        }

        return resource;
    }

} // namespace spieler::renderer
