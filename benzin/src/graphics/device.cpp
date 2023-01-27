#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/device.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "benzin/core/common.hpp"

#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/graphics/graphics_command_list.hpp"
#include "benzin/graphics/descriptor_manager.hpp"
#include "benzin/graphics/resource_view_builder.hpp"
#include "benzin/graphics/resource_loader.hpp"
#include "benzin/graphics/utils.hpp"

namespace benzin
{
    namespace
    {
        
        void EnableD3D12DebugLayer()
        {
#if defined(BENZIN_DEBUG)
            ComPtr<ID3D12Debug> d3d12Debug;
            BENZIN_D3D12_ASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));
            d3d12Debug->EnableDebugLayer();

            ComPtr<ID3D12Debug3> d3d12Debug3;
            BENZIN_D3D12_ASSERT(d3d12Debug.As(&d3d12Debug3));
            d3d12Debug3->SetEnableGPUBasedValidation(false); // TODO: replace with true
            d3d12Debug3->SetEnableSynchronizedCommandQueueValidation(true);
#endif // defined(BENZIN_DEBUG)
        }

        void BreakOnD3D12Error(ID3D12Device* d3d12Device, bool isBreak)
        {
#if defined(BENZIN_DEBUG)
            {
                ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
                BENZIN_D3D12_ASSERT(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));

                d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, isBreak);
                d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, isBreak);
                d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, isBreak);
            }
#endif // defined(BENZIN_DEBUG)
        }

        void ReportLiveD3D12Objects(ID3D12Device* d3d12Device)
        {
#if defined(BENZIN_DEBUG)
            using namespace magic_enum::bitwise_operators;

            ComPtr<ID3D12DebugDevice> d3d12DebugDevice;
            BENZIN_D3D12_ASSERT(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12DebugDevice)));

            d3d12DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL);
#endif // defined(BENZIN_DEBUG)
        }

        D3D12_HEAP_PROPERTIES GetD3D12HeapProperties(D3D12_HEAP_TYPE d3d12Type)
        {
            return D3D12_HEAP_PROPERTIES
            {
                .Type{ d3d12Type },
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

        BufferResource::Config ValidateBufferResourceConfig(const BufferResource::Config& config)
        {
            using namespace magic_enum::bitwise_operators;

            BufferResource::Config validatedConfig{ config };

            if ((validatedConfig.Flags & BufferResource::Flags::ConstantBuffer) != BufferResource::Flags::None)
            {
                validatedConfig.ElementSize = utils::Align<uint32_t>(validatedConfig.ElementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            }

            return validatedConfig;
        }

        D3D12_RESOURCE_DESC ConvertToD3D12ResourceDesc(const TextureResource::Config& config)
        {
            return D3D12_RESOURCE_DESC
            {
                .Dimension{ static_cast<D3D12_RESOURCE_DIMENSION>(config.Type) },
                .Alignment{ 0 },
                .Width{ static_cast<uint64_t>(config.Width) },
                .Height{ config.Height },
                .DepthOrArraySize{ config.ArraySize },
                .MipLevels{ config.MipCount },
                .Format{ static_cast<DXGI_FORMAT>(config.Format) },
                .SampleDesc{ 1, 0 },
                .Layout{ D3D12_TEXTURE_LAYOUT_UNKNOWN },
                .Flags{ static_cast<D3D12_RESOURCE_FLAGS>(config.Flags) }
            };
        }

        D3D12_CLEAR_VALUE ConvertToD3D12ClearValue(const TextureResource::Config& config, const TextureResource::ClearColor& clearColor)
        {
            return D3D12_CLEAR_VALUE
            {
                .Format{ static_cast<DXGI_FORMAT>(config.Format) },
                .Color 
                { 
                    clearColor.Color.x,
                    clearColor.Color.y,
                    clearColor.Color.z,
                    clearColor.Color.w
                }
            };
        }

        D3D12_CLEAR_VALUE ConvertToD3D12ClearValue(const TextureResource::Config& config, const TextureResource::ClearDepthStencil& clearDepthStencil)
        {
            return D3D12_CLEAR_VALUE
            {
                .Format{ static_cast<DXGI_FORMAT>(config.Format) },
                .DepthStencil
                {
                    .Depth{ clearDepthStencil.Depth },
                    .Stencil{ clearDepthStencil.Stencil }
                }
            };
        }

        TextureResource::Config ConvertFromD3D12ResourceDesc(const D3D12_RESOURCE_DESC& d3d12ResourceDesc)
        {
            return TextureResource::Config
            {
                .Type{ static_cast<TextureResource::Type>(d3d12ResourceDesc.Dimension) },
                .Width{ static_cast<uint32_t>(d3d12ResourceDesc.Width) },
                .Height{ d3d12ResourceDesc.Height },
                .ArraySize{ d3d12ResourceDesc.DepthOrArraySize },
                .MipCount{ d3d12ResourceDesc.MipLevels },
                .Format{ static_cast<GraphicsFormat>(d3d12ResourceDesc.Format) },
                .Flags{ static_cast<TextureResource::Flags>(d3d12ResourceDesc.Flags) }
            };
        }

    } // anonymous namespace

    constexpr DescriptorManager::Config g_DescriptorManagerConfig
    {
        .RenderTargetViewDescriptorCount{ 100 },
        .DepthStencilViewDescriptorCount{ 100 },
        .ResourceDescriptorCount{ 100 },
        .SamplerDescriptorCount{ 100 }
    };

    Device::Device()
    {
        EnableD3D12DebugLayer();

        BENZIN_D3D12_ASSERT(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_D3D12Device)));
        SetName("MainDevice");

        BreakOnD3D12Error(m_D3D12Device, true);

        m_DescriptorManager = new DescriptorManager{ *this, g_DescriptorManagerConfig };
        m_ResourceLoader = new ResourceLoader{ *this };
        m_ResourceViewBuilder = new ResourceViewBuilder{ *this };

        m_ResourceReleaser = [&](Resource* resource)
        {
            BENZIN_ASSERT(m_DescriptorManager);

            resource->ReleaseViews(*m_DescriptorManager);

            delete resource;
        };

        BENZIN_INFO("Device created");
    }

    Device::~Device()
    {
        delete m_ResourceViewBuilder;
        delete m_ResourceLoader;
        delete m_DescriptorManager;

        BreakOnD3D12Error(m_D3D12Device, false);
        ReportLiveD3D12Objects(m_D3D12Device);
        SafeReleaseD3D12Object(m_D3D12Device);

        BENZIN_INFO("Device destroyed");
    }

    std::shared_ptr<BufferResource> Device::CreateBufferResource(const BufferResource::Config& config) const
    {
        BENZIN_ASSERT(config.ElementSize != 0);
        BENZIN_ASSERT(config.ElementCount != 0);

        const D3D12_HEAP_TYPE d3d12HeapType = config.Flags == BufferResource::Flags::None ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ConvertToD3D12ResourceDesc(config);

        auto* rawBufferResource = new BufferResource
        {
            CreateD3D12CommittedResource(d3d12HeapType, &d3d12ResourceDesc, nullptr),
            ValidateBufferResourceConfig(config)
        };

        return std::shared_ptr<BufferResource>{ rawBufferResource, m_ResourceReleaser };
    }

    std::shared_ptr<TextureResource> Device::RegisterTextureResource(ID3D12Resource* d3d12Resource) const
    {
        BENZIN_ASSERT(d3d12Resource);

        auto* rawTextureResource = new TextureResource
        {
            d3d12Resource,
            ConvertFromD3D12ResourceDesc(d3d12Resource->GetDesc())
        };

        return std::shared_ptr<TextureResource>{ rawTextureResource, m_ResourceReleaser };
    }

    std::shared_ptr<TextureResource> Device::CreateTextureResource(const TextureResource::Config& config) const
    {
        const D3D12_HEAP_TYPE d3d12HeapType = D3D12_HEAP_TYPE_DEFAULT;
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ConvertToD3D12ResourceDesc(config);

        auto* rawTextureResource = new TextureResource
        {
            CreateD3D12CommittedResource(d3d12HeapType, &d3d12ResourceDesc, nullptr),
            config
        };

        return std::shared_ptr<TextureResource>{ rawTextureResource, m_ResourceReleaser };
    }

    std::shared_ptr<TextureResource> Device::CreateTextureResource(const TextureResource::Config& config, const TextureResource::ClearColor& clearColor) const
    {
        const D3D12_HEAP_TYPE d3d12HeapType = D3D12_HEAP_TYPE_DEFAULT;
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ConvertToD3D12ResourceDesc(config);
        const D3D12_CLEAR_VALUE d3d12ClearValue = ConvertToD3D12ClearValue(config, clearColor);

        auto* rawTextureResource = new TextureResource
        {
            CreateD3D12CommittedResource(d3d12HeapType, &d3d12ResourceDesc, &d3d12ClearValue),
            config
        };

        return std::shared_ptr<TextureResource>{ rawTextureResource, m_ResourceReleaser };
    }

    std::shared_ptr<TextureResource> Device::CreateTextureResource(const TextureResource::Config& config, const TextureResource::ClearDepthStencil& clearDepthStencil) const
    {
        const D3D12_HEAP_TYPE d3d12HeapType = D3D12_HEAP_TYPE_DEFAULT;
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ConvertToD3D12ResourceDesc(config);
        const D3D12_CLEAR_VALUE d3d12ClearValue = ConvertToD3D12ClearValue(config, clearDepthStencil);

        auto* rawTextureResource = new TextureResource
        {
            CreateD3D12CommittedResource(d3d12HeapType, &d3d12ResourceDesc, &d3d12ClearValue),
            config
        };

        return std::shared_ptr<TextureResource>{ rawTextureResource, m_ResourceReleaser };
    }

    ID3D12Resource* Device::CreateD3D12CommittedResource(
        D3D12_HEAP_TYPE d3d12HeapType,
        const D3D12_RESOURCE_DESC* d3d12ResourceDesc,
        const D3D12_CLEAR_VALUE* d3d12ClearValueDesc
    ) const
    {
        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(d3d12HeapType);

        ID3D12Resource* d3d12Resource = nullptr;

        BENZIN_D3D12_ASSERT(m_D3D12Device->CreateCommittedResource(
            &d3d12HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            d3d12ResourceDesc,
            d3d12HeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON,
            d3d12ClearValueDesc,
            IID_PPV_ARGS(&d3d12Resource)
        ));

        return d3d12Resource;
    }

} // namespace benzin
