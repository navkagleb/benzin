#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    namespace
    {

        D3D12_HEAP_PROPERTIES GetDefaultD3D12HeapProperties()
        {
            return D3D12_HEAP_PROPERTIES
            {
                .Type = D3D12_HEAP_TYPE_DEFAULT,
                .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                .CreationNodeMask = 1,
                .VisibleNodeMask = 1,
            };
        }

        D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const TextureCreation& textureCreation)
        {
            D3D12_RESOURCE_FLAGS d3d12ResourceFlags = D3D12_RESOURCE_FLAG_NONE;

            if (textureCreation.Flags[TextureFlag::AllowRenderTarget])
            {
                d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            }

            if (textureCreation.Flags[TextureFlag::AllowDepthStencil])
            {
                d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            }

            if (textureCreation.Flags[TextureFlag::AllowUnorderedAccess])
            {
                d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }

            return D3D12_RESOURCE_DESC
            {
                .Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(textureCreation.Type),
                .Alignment = 0,
                .Width = static_cast<uint64_t>(textureCreation.Width),
                .Height = textureCreation.Height,
                .DepthOrArraySize = textureCreation.ArraySize,
                .MipLevels = textureCreation.MipCount,
                .Format = static_cast<DXGI_FORMAT>(textureCreation.Format),
                .SampleDesc{ 1, 0 },
                .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                .Flags = d3d12ResourceFlags,
            };
        }

        void CreateD3D12Resource(const TextureCreation& textureCreation, ID3D12Device* d3d12Device, ID3D12Resource*& d3d12Resource)
        {
            BenzinAssert(d3d12Device);

            const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
            const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(textureCreation);

            if (textureCreation.Flags[TextureFlag::AllowRenderTarget] || textureCreation.Flags[TextureFlag::AllowDepthStencil])
            {
                D3D12_CLEAR_VALUE d3d12ClearValue{ .Format = static_cast<DXGI_FORMAT>(textureCreation.Format) };

                if (textureCreation.Flags[TextureFlag::AllowRenderTarget])
                {
                    memcpy(&d3d12ClearValue.Color, &textureCreation.ClearValue.Color, sizeof(textureCreation.ClearValue.Color));
                }
                else if (textureCreation.Flags[TextureFlag::AllowDepthStencil])
                {
                    memcpy(&d3d12ClearValue.DepthStencil, &textureCreation.ClearValue.DepthStencil, sizeof(textureCreation.ClearValue.DepthStencil));
                }

                BenzinAssert(d3d12Device->CreateCommittedResource(
                    &d3d12HeapProperties,
                    D3D12_HEAP_FLAG_NONE,
                    &d3d12ResourceDesc,
                    static_cast<D3D12_RESOURCE_STATES>(textureCreation.InitialState),
                    &d3d12ClearValue,
                    IID_PPV_ARGS(&d3d12Resource)
                ));
            }
            else
            {
                BenzinAssert(d3d12Device->CreateCommittedResource(
                    &d3d12HeapProperties,
                    D3D12_HEAP_FLAG_NONE,
                    &d3d12ResourceDesc,
                    static_cast<D3D12_RESOURCE_STATES>(textureCreation.InitialState),
                    nullptr,
                    IID_PPV_ARGS(&d3d12Resource)
                ));
            }
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResourceViewDesc(TextureType textureType, const TextureShaderResourceViewCreation& creation)
        {
            const bool isArrayTexture = creation.ArraySize > 1;

            D3D12_SHADER_RESOURCE_VIEW_DESC d3d12ShaderResourceViewDesc
            {
                .Format = static_cast<DXGI_FORMAT>(creation.Format),
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            };

            switch (textureType)
            {
                using enum TextureType;

                case Texture2D:
                {
                    if (!isArrayTexture)
                    {
                        d3d12ShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        d3d12ShaderResourceViewDesc.Texture2D = D3D12_TEX2D_SRV
                        {
                            .MostDetailedMip = creation.MostDetailedMipIndex,
                            .MipLevels = creation.MipCount,
                            .PlaneSlice = 0,
                            .ResourceMinLODClamp = 0.0f,
                        };
                    }
                    else if (creation.IsCubeMap)
                    {
                        d3d12ShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                        d3d12ShaderResourceViewDesc.TextureCube = D3D12_TEXCUBE_SRV
                        {
                            .MostDetailedMip = creation.MostDetailedMipIndex,
                            .MipLevels = creation.MipCount,
                            .ResourceMinLODClamp = 0.0f,
                        };
                    }
                    else
                    {
                        d3d12ShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                        d3d12ShaderResourceViewDesc.Texture2DArray = D3D12_TEX2D_ARRAY_SRV
                        {
                            .MostDetailedMip = creation.MostDetailedMipIndex,
                            .MipLevels = creation.MipCount,
                            .FirstArraySlice = creation.FirstArrayIndex,
                            .ArraySize = creation.ArraySize,
                            .PlaneSlice = 0,
                            .ResourceMinLODClamp = 0.0f,
                        };
                    }

                    break;
                }
                default:
                {
                    std::unreachable();
                }
            }

            return d3d12ShaderResourceViewDesc;
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC ToD3D12UnorderedAccessViewDesc(TextureType textureType, const TextureUnorderedAccessViewCreation& creation)
        {
            const bool isArrayTexture = creation.ArraySize > 1;

            D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UnorderedAccessViewDesc{ .Format = static_cast<DXGI_FORMAT>(creation.Format) };

            switch (textureType)
            {
                using enum TextureType;

                case Texture2D:
                {
                    if (!isArrayTexture)
                    {
                        d3d12UnorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                        d3d12UnorderedAccessViewDesc.Texture2D = D3D12_TEX2D_UAV
                        {
                            .MipSlice = 0,
                            .PlaneSlice = 0,
                        };
                    }
                    else
                    {
                        d3d12UnorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                        d3d12UnorderedAccessViewDesc.Texture2DArray = D3D12_TEX2D_ARRAY_UAV
                        {
                            .MipSlice = 0,
                            .FirstArraySlice = creation.FirstArrayIndex,
                            .ArraySize = creation.ArraySize,
                            .PlaneSlice = 0,
                        };
                    }

                    break;
                }
                default:
                {
                    std::unreachable();
                }
            }

            return d3d12UnorderedAccessViewDesc;
        }

        D3D12_RENDER_TARGET_VIEW_DESC ToD3D12RenderTargetViewDesc(TextureType textureType, const TextureRenderTargetViewCreation& creation)
        {
            const bool isArrayTexture = creation.ArraySize > 1;

            D3D12_RENDER_TARGET_VIEW_DESC d3d12RenderTargetViewDesc{ .Format = static_cast<DXGI_FORMAT>(creation.Format) };

            switch (textureType)
            {
                using enum TextureType;

                case Texture2D:
                {
                    if (!isArrayTexture)
                    {
                        d3d12RenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                        d3d12RenderTargetViewDesc.Texture2D = D3D12_TEX2D_RTV
                        {
                            .MipSlice = 0,
                            .PlaneSlice = 0,
                        };
                    }
                    else
                    {
                        d3d12RenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                        d3d12RenderTargetViewDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
                        {
                            .MipSlice = 0,
                            .FirstArraySlice = creation.FirstArrayIndex,
                            .ArraySize = creation.ArraySize,
                            .PlaneSlice = 0,
                        };
                    }

                    break;
                }
                default:
                {
                    std::unreachable();
                }
            }

            return d3d12RenderTargetViewDesc;
        }

    } // anonymous namespace

    Texture::Texture(Device& device, const TextureCreation& creation)
        : Resource{ device }
    {
        CreateD3D12Resource(creation, m_Device.GetD3D12Device(), m_D3D12Resource);
        m_Type = creation.Type;
        m_IsCubeMap = creation.IsCubeMap;
        m_Format = creation.Format;
        m_Width = creation.Width;
        m_Height = creation.Height;
        m_ArraySize = creation.ArraySize;
        m_MipCount = creation.MipCount;

        if (!creation.DebugName.IsEmpty())
        {
            SetD3D12ObjectDebugName(m_D3D12Resource, creation.DebugName);
        }

        if (creation.IsNeedShaderResourceView)
        {
            PushShaderResourceView();
        }

        if (creation.IsNeedUnorderedAccessView)
        {
            PushUnorderedAccessView();
        }

        if (creation.IsNeedRenderTargetView)
        {
            PushRenderTargetView();
        }

        if (creation.IsNeedDepthStencilView)
        {
            PushDepthStencilView();
        }
    }

    Texture::Texture(Device& device, ID3D12Resource* d3d12Resource)
        : Resource{ device }
    {
        BenzinAssert(d3d12Resource);
        m_D3D12Resource = d3d12Resource;

        {
            const D3D12_RESOURCE_DESC d3d12ResourceDesc = d3d12Resource->GetDesc();
            m_Type = TextureType{ d3d12ResourceDesc.Dimension };
            m_Format = GraphicsFormat{ d3d12ResourceDesc.Format };
            m_Width = static_cast<uint32_t>(d3d12ResourceDesc.Width);
            m_Height = d3d12ResourceDesc.Height;
            m_ArraySize = d3d12ResourceDesc.DepthOrArraySize;
            m_MipCount = d3d12ResourceDesc.MipLevels;
        }
    }

    uint32_t Texture::GetSizeInBytes() const
    {
        BenzinAssert(m_D3D12Resource);

        const D3D12_RESOURCE_DESC d3d12ResourceDesc = m_D3D12Resource->GetDesc();
        const uint32_t firstSubResource = 0;
        const uint32_t subResourceCount = GetSubResourceCount();

        uint64_t sizeInBytes = 0;
        m_Device.GetD3D12Device()->GetCopyableFootprints(
            &d3d12ResourceDesc,
            firstSubResource,
            subResourceCount,
            0,
            nullptr,
            nullptr,
            nullptr,
            &sizeInBytes
        );

        BenzinAssert(sizeInBytes != 0);
        BenzinAssert(sizeInBytes <= std::numeric_limits<uint32_t>::max());
        return static_cast<uint32_t>(sizeInBytes);
    }

    uint32_t Texture::GetSubResourceCount() const
    {
        return m_MipCount * m_ArraySize * m_Device.GetPlaneCountFromFormat(m_Format);
    }

    uint32_t Texture::PushShaderResourceView(const TextureShaderResourceViewCreation& creation)
    {
        BenzinAssert(m_D3D12Resource);

        TextureShaderResourceViewCreation validatedCreataion = creation;
        validatedCreataion.Format = creation.Format != GraphicsFormat::Unknown ? creation.Format : m_Format;
        validatedCreataion.IsCubeMap = creation.IsCubeMap ? true : m_IsCubeMap;
        validatedCreataion.ArraySize = creation.ArraySize != 0 ? creation.ArraySize : m_ArraySize;

        const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12ShaderResourceViewDesc = ToD3D12ShaderResourceViewDesc(m_Type, validatedCreataion);

        const DescriptorType descriptorType = DescriptorType::ShaderResourceView;
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(descriptorType);

        m_Device.GetD3D12Device()->CreateShaderResourceView(
            m_D3D12Resource,
            &d3d12ShaderResourceViewDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(descriptorType, descriptor);
    }

    uint32_t Texture::PushUnorderedAccessView(const TextureUnorderedAccessViewCreation& creation)
    {
        BenzinAssert(m_D3D12Resource);

        TextureUnorderedAccessViewCreation validatedCreation = creation;
        validatedCreation.Format = creation.Format != GraphicsFormat::Unknown ? creation.Format : m_Format;
        validatedCreation.ArraySize = creation.ArraySize != 0 ? creation.ArraySize : m_ArraySize;

        const D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UnorderedAccessViewDesc = ToD3D12UnorderedAccessViewDesc(m_Type, validatedCreation);

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

    uint32_t Texture::PushRenderTargetView(const TextureRenderTargetViewCreation& creation)
    {
        BenzinAssert(m_D3D12Resource);

        TextureRenderTargetViewCreation validatedCreation = creation;
        validatedCreation.Format = creation.Format != GraphicsFormat::Unknown ? creation.Format : m_Format;
        validatedCreation.ArraySize = creation.ArraySize != 0 ? creation.ArraySize : m_ArraySize;

        const D3D12_RENDER_TARGET_VIEW_DESC d3d12RenderTargetViewDesc = ToD3D12RenderTargetViewDesc(m_Type, validatedCreation);

        const DescriptorType descriptorType = DescriptorType::RenderTargetView;
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(descriptorType);

        m_Device.GetD3D12Device()->CreateRenderTargetView(
            m_D3D12Resource,
            &d3d12RenderTargetViewDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(descriptorType, descriptor);
    }

    uint32_t Texture::PushDepthStencilView()
    {
        BenzinAssert(m_D3D12Resource);

        const DescriptorType descriptorType = DescriptorType::DepthStencilView;
        const Descriptor descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(descriptorType);

        m_Device.GetD3D12Device()->CreateDepthStencilView(
            m_D3D12Resource,
            nullptr, // Default D3D12_DEPTH_STENCIL_VIEW_DESC
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCPUHandle() }
        );

        return PushResourceView(descriptorType, descriptor);
    }

} // namespace benzin
