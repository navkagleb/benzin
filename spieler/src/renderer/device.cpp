#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/device.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "spieler/core/common.hpp"
#include "spieler/core/assert.hpp"
#include "spieler/core/logger.hpp"

#include "spieler/renderer/buffer.hpp"

namespace spieler::renderer
{

    namespace _internal
    {

        constexpr inline static uint32_t Align(uint32_t value, uint32_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        enum class HeapType : uint8_t
        {
            Default = D3D12_HEAP_TYPE_DEFAULT,
            Upload = D3D12_HEAP_TYPE_UPLOAD
        };

        inline static D3D12_HEAP_PROPERTIES GetD3D12HeapDesc(HeapType heapType)
        {
            return D3D12_HEAP_PROPERTIES
            {
                .Type = static_cast<D3D12_HEAP_TYPE>(heapType),
                .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                .CreationNodeMask = 1,
                .VisibleNodeMask = 1,
            };
        }

        inline static D3D12_RESOURCE_DESC ConvertToD3D12ResourceDesc(const BufferConfig& bufferConfig)
        {
            return D3D12_RESOURCE_DESC
            {
                .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                .Alignment = bufferConfig.Alignment,
                .Width = bufferConfig.ElementSize * bufferConfig.ElementCount,
                .Height = 1,
                .DepthOrArraySize = 1,
                .MipLevels = 1,
                .Format = DXGI_FORMAT_UNKNOWN,
                .SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 },
                .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                .Flags = D3D12_RESOURCE_FLAG_NONE
            };
        }

        inline static D3D12_RESOURCE_DESC ConvertToD3D12ResourceDesc(const Texture2DConfig& texture2DConfig)
        {
            return D3D12_RESOURCE_DESC
            {
                .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                .Alignment = 0,
                .Width = texture2DConfig.Width,
                .Height = texture2DConfig.Height,
                .DepthOrArraySize = 1,
                .MipLevels = 1,
                .Format = D3D12Converter::Convert(texture2DConfig.Format),
                .SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 },
                .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                .Flags = static_cast<D3D12_RESOURCE_FLAGS>(texture2DConfig.Flags)
            };
        }

        inline static D3D12_CLEAR_VALUE ConvertToD3D12ClearValue(const Texture2DConfig& texture2DConfig, const TextureClearValue& textureClearValue)
        {
            return D3D12_CLEAR_VALUE
            {
                .Format = D3D12Converter::Convert(texture2DConfig.Format),
                .Color = 
                { 
                    textureClearValue.Color.x,
                    textureClearValue.Color.y,
                    textureClearValue.Color.z,
                    textureClearValue.Color.w
                }
            };
        }

        inline static D3D12_CLEAR_VALUE ConvertToD3D12ClearValue(const Texture2DConfig& texture2DConfig, const DepthStencilClearValue& depthStencilClearValue)
        {
            const D3D12_DEPTH_STENCIL_VALUE d3d12DepthStencilClearValue
            {
                .Depth = depthStencilClearValue.Depth,
                .Stencil = depthStencilClearValue.Stencil
            };

            return D3D12_CLEAR_VALUE
            {
                .Format = D3D12Converter::Convert(texture2DConfig.Format),
                .DepthStencil = d3d12DepthStencilClearValue
            };
        }

    } // namespace _internal

    constexpr DescriptorManagerConfig g_DescriptorManagerConfig
    {
        .RTVDescriptorCount = 100,
        .DSVDescriptorCount = 100,
        .SamplerDescriptorCount = 100,
        .CBVDescriptorCount = 100,
        .SRVDescriptorCount = 100,
        .UAVDescriptorCount = 100
    };

    Device::Device()
    {
#if defined(SPIELER_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        SPIELER_ASSERT(SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), &debugController)));

        debugController->EnableDebugLayer();
    }
#endif

        // m_Device
        SPIELER_ASSERT(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), &m_Device)));

        // m_DescriptorManager
        m_DescriptorManager = DescriptorManager{ *this, g_DescriptorManagerConfig };
    }

    bool Device::CreateDefaultBuffer(const BufferConfig& bufferConfig, Resource& resource)
    {
        const D3D12_HEAP_PROPERTIES heapDesc{ _internal::GetD3D12HeapDesc(_internal::HeapType::Default) };
        const D3D12_RESOURCE_DESC resourceDesc{ _internal::ConvertToD3D12ResourceDesc(bufferConfig) };

        return CreateResource(&heapDesc, &resourceDesc, nullptr, resource.m_Resource);
    }

    bool Device::CreateUploadBuffer(const BufferConfig& bufferConfig, Resource& resource)
    {
        const D3D12_HEAP_PROPERTIES heapDesc{ _internal::GetD3D12HeapDesc(_internal::HeapType::Upload) };
        const D3D12_RESOURCE_DESC resourceDesc{ _internal::ConvertToD3D12ResourceDesc(bufferConfig) };

        return CreateResource(&heapDesc, &resourceDesc, nullptr, resource.m_Resource);
    }

    bool Device::CreateTexture(const Texture2DConfig& texture2DConfig, Texture2DResource& resource)
    {
        const D3D12_HEAP_PROPERTIES heapDesc{ _internal::GetD3D12HeapDesc(_internal::HeapType::Default) };
        const D3D12_RESOURCE_DESC resourceDesc{ _internal::ConvertToD3D12ResourceDesc(texture2DConfig) };

        SPIELER_RETURN_IF_FAILED(CreateResource(&heapDesc, &resourceDesc, nullptr, resource.m_Resource));

        resource.m_Config = texture2DConfig;

        return true;
    }

    bool Device::CreateTexture(const Texture2DConfig& texture2DConfig, const TextureClearValue& textureClearValue, Texture2DResource& resource)
    {
        const D3D12_HEAP_PROPERTIES heapDesc{ _internal::GetD3D12HeapDesc(_internal::HeapType::Default) };
        const D3D12_RESOURCE_DESC resourceDesc{ _internal::ConvertToD3D12ResourceDesc(texture2DConfig) };
        const D3D12_CLEAR_VALUE clearValueDesc{ _internal::ConvertToD3D12ClearValue(texture2DConfig, textureClearValue) };

        SPIELER_RETURN_IF_FAILED(CreateResource(&heapDesc, &resourceDesc, &clearValueDesc, resource.m_Resource));

        resource.m_Config = texture2DConfig;

        return true;
    }

    bool Device::CreateTexture(const Texture2DConfig& texture2DConfig, const DepthStencilClearValue& depthStencilClearValue, Texture2DResource& resource)
    {
        const D3D12_HEAP_PROPERTIES heapDesc{ _internal::GetD3D12HeapDesc(_internal::HeapType::Default) };
        const D3D12_RESOURCE_DESC resourceDesc{ _internal::ConvertToD3D12ResourceDesc(texture2DConfig) };
        const D3D12_CLEAR_VALUE clearValueDesc{ _internal::ConvertToD3D12ClearValue(texture2DConfig, depthStencilClearValue) };

        SPIELER_RETURN_IF_FAILED(CreateResource(&heapDesc, &resourceDesc, &clearValueDesc, resource.m_Resource));

        resource.m_Config = texture2DConfig;

        return true;
    }

    void Device::RegisterTexture(ComPtr<ID3D12Resource>&& nativeResource, Texture2DResource& resource)
    {
        const D3D12_RESOURCE_DESC nativeDesc{ nativeResource->GetDesc() };

        const Texture2DConfig texture2DConfig
        {
            .Width{ nativeDesc.Width },
            .Height{ nativeDesc.Height },
            .Format{ static_cast<GraphicsFormat>(nativeDesc.Format) },
            .Flags{ static_cast<Texture2DFlags>(nativeDesc.Flags) }
        };

        resource.m_Resource = std::move(nativeResource);
        resource.m_Config = texture2DConfig;
    }

    std::shared_ptr<BufferResource> Device::CreateBuffer(const BufferConfig& bufferConfig, BufferFlags bufferFlags)
    {
        using namespace magic_enum::bitwise_operators;

        if ((bufferFlags & BufferFlags::ConstantBuffer) != BufferFlags::None)
        {
            const_cast<BufferConfig&>(bufferConfig).ElementSize = _internal::Align(bufferConfig.ElementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        }

        std::shared_ptr<BufferResource> buffer{ std::make_shared<BufferResource>(bufferConfig) };

        if ((bufferFlags & BufferFlags::Dynamic) != BufferFlags::None)
        {
            SPIELER_ASSERT(CreateUploadBuffer(bufferConfig, *buffer));
        }
        else
        {
            SPIELER_ASSERT(CreateDefaultBuffer(bufferConfig, *buffer));
        }

        return buffer;
    }

    bool Device::CreateResource(
        const D3D12_HEAP_PROPERTIES* heapDesc, 
        const D3D12_RESOURCE_DESC* const resourceDesc, 
        const D3D12_CLEAR_VALUE* const clearValueDesc, 
        ComPtr<ID3D12Resource>& resource)
    {
        const ::HRESULT result
        {
            m_Device->CreateCommittedResource(
                heapDesc,
                D3D12_HEAP_FLAG_NONE,
                resourceDesc,
                heapDesc->Type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON,
                clearValueDesc,
                __uuidof(std::remove_reference<decltype(resource)>::type::InterfaceType),
                &resource
            )
        };

        if (FAILED(result))
        {
            SPIELER_CRITICAL("Failed to create Resource");
            return false;
        }

        return true;
    }

} // namespace spieler::renderer