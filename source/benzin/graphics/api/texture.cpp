#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/texture.hpp"

namespace benzin
{

    namespace
    {

        template <typename... Ts>
        struct MakeVisitor : Ts...
        { 
            using Ts::operator()...;
        };

        template <typename... Ts>
        MakeVisitor(Ts...) -> MakeVisitor<Ts...>;

        template <typename Variant, typename... Lambdas>
        auto Visit(const Variant& variant, Lambdas... lambdas)
        {
            return std::visit(MakeVisitor{ lambdas... }, variant);
        }

        D3D12_HEAP_PROPERTIES GetDefaultD3D12HeapProperties()
        {
            return D3D12_HEAP_PROPERTIES
            {
                .Type{ D3D12_HEAP_TYPE_DEFAULT },
                .CPUPageProperty{ D3D12_CPU_PAGE_PROPERTY_UNKNOWN },
                .MemoryPoolPreference{ D3D12_MEMORY_POOL_UNKNOWN },
                .CreationNodeMask{ 1 },
                .VisibleNodeMask{ 1 },
            };
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

        D3D12_CLEAR_VALUE ConvertToD3D12ClearValue(GraphicsFormat format, const TextureResource::ClearValue clearValue)
        {
            D3D12_CLEAR_VALUE d3d12ClearValue
            {
                .Format{ static_cast<DXGI_FORMAT>(format) }
            };

            Visit(
                clearValue,
                [&](const TextureResource::ClearColor& clearColor)
                {
                    d3d12ClearValue.Color[0] = clearColor.Color.x;
                    d3d12ClearValue.Color[1] = clearColor.Color.y;
                    d3d12ClearValue.Color[2] = clearColor.Color.z;
                    d3d12ClearValue.Color[3] = clearColor.Color.w;
                },
                [&](const TextureResource::ClearDepthStencil& clearDepthStencil)
                {
                    d3d12ClearValue.DepthStencil.Depth = clearDepthStencil.Depth;
                    d3d12ClearValue.DepthStencil.Stencil = clearDepthStencil.Stencil;
                }
            );

            return d3d12ClearValue;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC FillD3D12ShaderResourceViewDesc(
            TextureResource::Type type,
            DXGI_FORMAT dxgiFormat,
            bool isArray,
            bool isCubeMap,
            uint32_t mostDetailedMipIndex,
            uint32_t mipCount,
            uint32_t firstArrayIndex,
            uint32_t arraySize
        )
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SRVDesc
            {
                .Format{ dxgiFormat },
                .Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
            };

            switch (type)
            {
                using enum TextureResource::Type;

                case Texture2D:
                {
                    if (!isArray)
                    {
                        d3d12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        d3d12SRVDesc.Texture2D = D3D12_TEX2D_SRV
                        {
                            .MostDetailedMip{ mostDetailedMipIndex },
                            .MipLevels{ mipCount },
                            .PlaneSlice{ 0 },
                            .ResourceMinLODClamp{ 0.0f },
                        };
                    }
                    else if (isCubeMap)
                    {
                        d3d12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                        d3d12SRVDesc.TextureCube = D3D12_TEXCUBE_SRV
                        {
                            .MostDetailedMip{ mostDetailedMipIndex },
                            .MipLevels{ mipCount },
                            .ResourceMinLODClamp{ 0.0f },
                        };
                    }
                    else
                    {
                        d3d12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                        d3d12SRVDesc.Texture2DArray = D3D12_TEX2D_ARRAY_SRV
                        {
                            .MostDetailedMip{ mostDetailedMipIndex },
                            .MipLevels{ mipCount },
                            .FirstArraySlice{ firstArrayIndex },
                            .ArraySize{ arraySize },
                            .PlaneSlice{ 0 },
                            .ResourceMinLODClamp{ 0.0f },
                        };
                    }

                    break;
                }
                default:
                {
                    BENZIN_WARNING("Unsupported TextureResource::Type!");
                    BENZIN_ASSERT(false);

                    break;
                }
            }

            return d3d12SRVDesc;
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC FillD3D12UnorderedAccessViewDesc(
            TextureResource::Type type,
            DXGI_FORMAT dxgiFormat,
            bool isArray,
            uint32_t startElementIndex,
            uint32_t elementCount
        )
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UAVDesc
            {
                .Format{ dxgiFormat },
            };

            switch (type)
            {
                using enum TextureResource::Type;

                case Texture2D:
                {
                    if (!isArray)
                    {
                        d3d12UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                        d3d12UAVDesc.Texture2D = D3D12_TEX2D_UAV
                        {
                            .MipSlice{ 0 },
                            .PlaneSlice{ 0 },
                        };
                    }
                    else
                    {
                        d3d12UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                        d3d12UAVDesc.Texture2DArray = D3D12_TEX2D_ARRAY_UAV
                        {
                            .MipSlice{ 0 },
                            .FirstArraySlice{ startElementIndex },
                            .ArraySize{ elementCount },
                            .PlaneSlice{ 0 },
                        };
                    }

                    break;
                }
                default:
                {
                    BENZIN_WARNING("Unsupported TextureResource::Type!");
                    BENZIN_ASSERT(false);

                    break;
                }
            }

            return d3d12UAVDesc;
        }

        D3D12_RENDER_TARGET_VIEW_DESC FillD3D12RenderTargetViewDesc(
            TextureResource::Type type,
            DXGI_FORMAT dxgiFormat,
            bool isArray,
            uint32_t startElementIndex,
            uint32_t elementCount
        )
        {
            D3D12_RENDER_TARGET_VIEW_DESC d3d12RTVDesc
            {
                .Format{ dxgiFormat },
            };

            switch (type)
            {
                using enum TextureResource::Type;

                case Texture2D:
                {
                    if (!isArray)
                    {
                        d3d12RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                        d3d12RTVDesc.Texture2D = D3D12_TEX2D_RTV
                        {
                            .MipSlice{ 0 },
                            .PlaneSlice{ 0 },
                        };
                    }
                    else
                    {
                        d3d12RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                        d3d12RTVDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
                        {
                            .MipSlice{ 0 },
                            .FirstArraySlice{ startElementIndex },
                            .ArraySize{ elementCount },
                            .PlaneSlice{ 0 },
                        };
                    }

                    break;
                }
                default:
                {
                    BENZIN_WARNING("Unsupported TextureResource::Type!");
                    BENZIN_ASSERT(false);

                    break;
                }
            }

            return d3d12RTVDesc;
        }

    } // anonymous namespace

    TextureResource::TextureResource(Device& device, const Config& config, std::optional<ClearValue> optimizedClearValue)
        : Resource{ device }
        , m_Config{ config }
    {
        BENZIN_ASSERT(device.GetD3D12Device());

        BENZIN_ASSERT(config.Type != Type::Unknown);
        BENZIN_ASSERT(config.Width != 0);
        BENZIN_ASSERT(config.Height != 0);
        BENZIN_ASSERT(config.ArraySize != 0);
        BENZIN_ASSERT(config.Format != GraphicsFormat::Unknown);

        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetDefaultD3D12HeapProperties();
        const D3D12_HEAP_FLAGS d3d12HeapFlags = D3D12_HEAP_FLAG_NONE;

        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ConvertToD3D12ResourceDesc(m_Config);
        const D3D12_RESOURCE_STATES d3d12InitialResourceState = D3D12_RESOURCE_STATE_COMMON;

        if (optimizedClearValue)
        {
            m_OptimizedClearValue = optimizedClearValue;
        }
        else
        {
            using namespace magic_enum::bitwise_operators;

            if ((m_Config.Flags & Flags::BindAsRenderTarget) != Flags::None)
            {
                m_OptimizedClearValue = ClearColor{};
            }
            else if ((m_Config.Flags & Flags::BindAsDepthStencil) != Flags::None)
            {
                m_OptimizedClearValue = ClearDepthStencil{};
            }
        }

        if (!m_OptimizedClearValue)
        {
            BENZIN_HR_ASSERT(m_Device.GetD3D12Device()->CreateCommittedResource(
                &d3d12HeapProperties,
                d3d12HeapFlags,
                &d3d12ResourceDesc,
                d3d12InitialResourceState,
                nullptr,
                IID_PPV_ARGS(&m_D3D12Resource)
            ));
        }
        else
        {
            const D3D12_CLEAR_VALUE d3d12OptimizedClearValue = ConvertToD3D12ClearValue(m_Config.Format, *m_OptimizedClearValue);

            BENZIN_HR_ASSERT(m_Device.GetD3D12Device()->CreateCommittedResource(
                &d3d12HeapProperties,
                d3d12HeapFlags,
                &d3d12ResourceDesc,
                d3d12InitialResourceState,
                &d3d12OptimizedClearValue,
                IID_PPV_ARGS(&m_D3D12Resource)
            ));
        }
    }

    TextureResource::TextureResource(Device& device, ID3D12Resource* d3d12Resource)
        : Resource{ device }
    {
        BENZIN_ASSERT(d3d12Resource);

        m_D3D12Resource = d3d12Resource;
        m_Config = ConvertFromD3D12ResourceDesc(d3d12Resource->GetDesc());
    }

    bool TextureResource::HasRenderTargetView(uint32_t index) const
    {
        return HasResourceView(Descriptor::Type::RenderTargetView, index);
    }

    bool TextureResource::HasDepthStencilView(uint32_t index) const
    {
        return HasResourceView(Descriptor::Type::DepthStencilView, index);
    }

    const Descriptor& TextureResource::GetRenderTargetView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::RenderTargetView, index);
    }

    const Descriptor& TextureResource::GetDepthStencilView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::DepthStencilView, index);
    }

    uint32_t TextureResource::PushShaderResourceView(const TextureShaderResourceViewConfig& config)
    {
        using magic_enum::enum_cast;
        using magic_enum::enum_integer;

        BENZIN_ASSERT(m_D3D12Resource);

        const GraphicsFormat format = config.Format != GraphicsFormat::Unknown ? config.Format : m_Config.Format;
        BENZIN_ASSERT(format != GraphicsFormat::Unknown);

        const bool isArray = m_Config.ArraySize != 1;
        const bool isCubeMap = config.IsCubeMap ? true : m_Config.IsCubeMap;

        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SRVDesc = FillD3D12ShaderResourceViewDesc(
            m_Config.Type,
            enum_cast<DXGI_FORMAT>(enum_integer(format)).value_or(DXGI_FORMAT_UNKNOWN),
            isArray,
            isCubeMap,
            config.MostDetailedMipIndex,
            config.MipCount,
            config.FirstArrayIndex,
            config.ArraySize
        );

        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::ShaderResourceView);

        m_Device.GetD3D12Device()->CreateShaderResourceView(
            m_D3D12Resource,
            &d3d12SRVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(Descriptor::Type::ShaderResourceView, descriptor);
    }

    uint32_t TextureResource::PushUnorderedAccessView(const TextureUnorderedAccessViewConfig& config)
    {
        using namespace magic_enum::bitwise_operators;
        using magic_enum::enum_cast;
        using magic_enum::enum_integer;

        BENZIN_ASSERT(m_D3D12Resource);
        BENZIN_ASSERT((m_Config.Flags & Flags::BindAsUnorderedAccess) != Flags::None);

        const GraphicsFormat format = config.Format != GraphicsFormat::Unknown ? config.Format : m_Config.Format;
        BENZIN_ASSERT(format != GraphicsFormat::Unknown);

        const bool isArray = m_Config.ArraySize != 1;

        const D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UAVDesc = FillD3D12UnorderedAccessViewDesc(
            m_Config.Type,
            enum_cast<DXGI_FORMAT>(enum_integer(format)).value_or(DXGI_FORMAT_UNKNOWN),
            isArray,
            config.StartElementIndex,
            config.ElementCount
        );

        ID3D12Resource* d3d12CounterResource = nullptr;

        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::UnorderedAccessView);

        m_Device.GetD3D12Device()->CreateUnorderedAccessView(
            m_D3D12Resource,
            d3d12CounterResource,
            &d3d12UAVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(Descriptor::Type::UnorderedAccessView, descriptor);
    }

    uint32_t TextureResource::PushRenderTargetView(const TextureRenderTargetViewConfig& config)
    {
        using namespace magic_enum::bitwise_operators;
        using magic_enum::enum_cast;
        using magic_enum::enum_integer;

        BENZIN_ASSERT(m_D3D12Resource);
        BENZIN_ASSERT((m_Config.Flags & Flags::BindAsRenderTarget) != Flags::None);

        const GraphicsFormat format = config.Format != GraphicsFormat::Unknown ? config.Format : m_Config.Format;
        BENZIN_ASSERT(format != GraphicsFormat::Unknown);

        const bool isArray = m_Config.ArraySize != 1;

        const D3D12_RENDER_TARGET_VIEW_DESC d3d12RTVDesc = FillD3D12RenderTargetViewDesc(
            m_Config.Type,
            enum_cast<DXGI_FORMAT>(enum_integer(format)).value_or(DXGI_FORMAT_UNKNOWN),
            isArray,
            config.StartElementIndex,
            config.ElementCount
        );

        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::RenderTargetView);

        m_Device.GetD3D12Device()->CreateRenderTargetView(
            m_D3D12Resource,
            &d3d12RTVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(Descriptor::Type::RenderTargetView, descriptor);
    }

    uint32_t TextureResource::PushDepthStencilView()
    {
        using namespace magic_enum::bitwise_operators;

        BENZIN_ASSERT(m_D3D12Resource);

        BENZIN_ASSERT((m_Config.Flags & Flags::BindAsDepthStencil) != Flags::None);

        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::DepthStencilView);

        const D3D12_DEPTH_STENCIL_VIEW_DESC* d3d12DSVDesc = nullptr;
        m_Device.GetD3D12Device()->CreateDepthStencilView(
            m_D3D12Resource,
            d3d12DSVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(Descriptor::Type::DepthStencilView, descriptor);
    }

} // namespace benzin
