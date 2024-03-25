#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/texture.hpp"

#include "benzin/core/asserter.hpp"

namespace benzin
{

    struct TextureDsv {};

    static D3D12_HEAP_PROPERTIES GetDefaultD3D12HeapProperties()
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

    static D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const TextureCreation& textureCreation)
    {
        D3D12_RESOURCE_FLAGS d3d12ResourceFlags = D3D12_RESOURCE_FLAG_NONE;

        if (textureCreation.Flags.IsSet(TextureFlag::AllowRenderTarget))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }

        if (textureCreation.Flags.IsSet(TextureFlag::AllowDepthStencil))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        if (textureCreation.Flags.IsSet(TextureFlag::AllowUnorderedAccess))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        return D3D12_RESOURCE_DESC
        {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D, // For now only 2D textured supported
            .Alignment = 0,
            .Width = (uint64_t)textureCreation.Width,
            .Height = textureCreation.Height,
            .DepthOrArraySize = textureCreation.Depth,
            .MipLevels = textureCreation.MipCount,
            .Format = (DXGI_FORMAT)textureCreation.Format,
            .SampleDesc{ 1, 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = d3d12ResourceFlags,
        };
    }

    static void CreateD3D12Resource(const TextureCreation& textureCreation, const Device& device, ID3D12Resource*& d3d12Resource)
    {
        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(textureCreation);

        if (textureCreation.Flags.IsAnySet(TextureFlag::AllowRenderTarget | TextureFlag::AllowDepthStencil))
        {
            D3D12_CLEAR_VALUE d3d12ClearValue{ .Format = (DXGI_FORMAT)textureCreation.Format };

            if (textureCreation.Flags.IsSet(TextureFlag::AllowRenderTarget))
            {
                memcpy(&d3d12ClearValue.Color, &g_DefaultClearColor, sizeof(g_DefaultClearColor));
            }
            else if (textureCreation.Flags.IsSet(TextureFlag::AllowDepthStencil))
            {
                memcpy(&d3d12ClearValue.DepthStencil, &g_DefaultClearDepthStencil, sizeof(g_DefaultClearDepthStencil));
            }

            BenzinEnsure(device.GetD3D12Device()->CreateCommittedResource(
                &d3d12HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &d3d12ResourceDesc,
                (D3D12_RESOURCE_STATES)textureCreation.InitialState,
                &d3d12ClearValue,
                IID_PPV_ARGS(&d3d12Resource)
            ));
        }
        else
        {
            BenzinEnsure(device.GetD3D12Device()->CreateCommittedResource(
                &d3d12HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &d3d12ResourceDesc,
                (D3D12_RESOURCE_STATES)textureCreation.InitialState,
                nullptr,
                IID_PPV_ARGS(&d3d12Resource)
            ));
        }

        BenzinEnsure(d3d12Resource);
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResourceViewDesc(const TextureSrv& textureSrv)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc
        {
            .Format = (DXGI_FORMAT)textureSrv.Format,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        };

        const bool isArrayTexture = textureSrv.DepthRange.Count > 1;
        if (!isArrayTexture)
        {
            d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            d3d12SrvDesc.Texture2D = D3D12_TEX2D_SRV
            {
                .MostDetailedMip = textureSrv.MostDetailedMipIndex,
                .MipLevels = textureSrv.MipCount,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0.0f,
            };
        }
        else if (textureSrv.IsCubeMap)
        {
            d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            d3d12SrvDesc.TextureCube = D3D12_TEXCUBE_SRV
            {
                .MostDetailedMip = textureSrv.MostDetailedMipIndex,
                .MipLevels = textureSrv.MipCount,
                .ResourceMinLODClamp = 0.0f,
            };
        }
        else
        {
            d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            d3d12SrvDesc.Texture2DArray = D3D12_TEX2D_ARRAY_SRV
            {
                .MostDetailedMip = textureSrv.MostDetailedMipIndex,
                .MipLevels = textureSrv.MipCount,
                .FirstArraySlice = textureSrv.DepthRange.StartIndex,
                .ArraySize = textureSrv.DepthRange.Count,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0.0f,
            };
        }

        return d3d12SrvDesc;
    }

    static D3D12_UNORDERED_ACCESS_VIEW_DESC ToD3D12UnorderedAccessViewDesc(const TextureUav& textureUav)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc{ .Format = (DXGI_FORMAT)textureUav.Format };

        const bool isArrayTexture = textureUav.DepthRange.Count > 1;
        if (!isArrayTexture)
        {
            d3d12UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            d3d12UavDesc.Texture2D = D3D12_TEX2D_UAV
            {
                .MipSlice = 0,
                .PlaneSlice = 0,
            };
        }
        else
        {
            d3d12UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            d3d12UavDesc.Texture2DArray = D3D12_TEX2D_ARRAY_UAV
            {
                .MipSlice = 0,
                .FirstArraySlice = textureUav.DepthRange.StartIndex,
                .ArraySize = textureUav.DepthRange.Count,
                .PlaneSlice = 0,
            };
        }

        return d3d12UavDesc;
    }

    static D3D12_RENDER_TARGET_VIEW_DESC ToD3D12RenderTargetViewDesc(const TextureRtv& textureRtv)
    {
        D3D12_RENDER_TARGET_VIEW_DESC d3d12RtvDesc{ .Format = (DXGI_FORMAT)textureRtv.Format };

        const bool isArrayTexture = textureRtv.DepthRange.Count > 1;
        if (!isArrayTexture)
        {
            d3d12RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            d3d12RtvDesc.Texture2D = D3D12_TEX2D_RTV
            {
                .MipSlice = 0,
                .PlaneSlice = 0,
            };
        }
        else
        {
            d3d12RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            d3d12RtvDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
            {
                .MipSlice = 0,
                .FirstArraySlice = textureRtv.DepthRange.StartIndex,
                .ArraySize = textureRtv.DepthRange.Count,
                .PlaneSlice = 0,
            };
        }

        return d3d12RtvDesc;
    }

    //

    Texture::Texture(Device& device, const TextureCreation& creation)
        : Resource{ device }
    {
        CreateD3D12Resource(creation, m_Device, m_D3D12Resource);
        SetD3D12ObjectDebugName(m_D3D12Resource, creation.DebugName);

        m_IsCubeMap = creation.IsCubeMap;
        m_Format = creation.Format;
        m_Width = creation.Width;
        m_Height = creation.Height;
        m_Depth = creation.Depth;
        m_MipCount = creation.MipCount;
    }

    Texture::Texture(Device& device, ID3D12Resource* d3d12Resource)
        : Resource{ device }
    {
        BenzinAssert(d3d12Resource);
        m_D3D12Resource = d3d12Resource;

        {
            const D3D12_RESOURCE_DESC d3d12ResourceDesc = d3d12Resource->GetDesc();
            m_Format = (GraphicsFormat)d3d12ResourceDesc.Format;
            m_Width = (uint32_t)d3d12ResourceDesc.Width;
            m_Height = d3d12ResourceDesc.Height;
            m_Depth = d3d12ResourceDesc.DepthOrArraySize;
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

        BenzinAssert(sizeInBytes != 0 && sizeInBytes <= std::numeric_limits<uint32_t>::max());
        return (uint32_t)sizeInBytes;
    }

    uint32_t Texture::GetSubResourceCount() const
    {
        return m_MipCount * m_Depth * m_Device.GetPlaneCountFromFormat(m_Format);
    }

    const Descriptor& Texture::GetSrv(const TextureSrv& textureSrv) const
    {
        BenzinAssert(m_D3D12Resource);
        BenzinAssert(textureSrv.DepthRange.Count < m_Depth);

        auto validatedSrv = textureSrv;
        validatedSrv.Format = textureSrv.Format != GraphicsFormat::Unknown ? textureSrv.Format : m_Format;
        validatedSrv.IsCubeMap = textureSrv.IsCubeMap ? true : m_IsCubeMap;
        validatedSrv.DepthRange.Count = textureSrv.DepthRange.Count != 0 ? textureSrv.DepthRange.Count : m_Depth;

        auto& descriptor = GetViewDescriptor(validatedSrv);
        if (!descriptor.IsCpuValid())
        {
            descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Srv);

            const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = ToD3D12ShaderResourceViewDesc(validatedSrv);

            m_Device.GetD3D12Device()->CreateShaderResourceView(
                m_D3D12Resource,
                &d3d12SrvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
            );
        }

        return descriptor;
    }

    const Descriptor& Texture::GetUav(const TextureUav& textureUav) const
    {
        BenzinAssert(m_D3D12Resource);
        BenzinAssert(textureUav.DepthRange.Count < m_Depth);

        auto validatedUav = textureUav;
        validatedUav.Format = textureUav.Format != GraphicsFormat::Unknown ? textureUav.Format : m_Format;
        validatedUav.DepthRange.Count = textureUav.DepthRange.Count != 0 ? textureUav.DepthRange.Count : m_Depth;

        auto& descriptor = GetViewDescriptor(validatedUav);
        if (!descriptor.IsCpuValid())
        {
            descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Uav);

            const D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc = ToD3D12UnorderedAccessViewDesc(validatedUav);
            m_Device.GetD3D12Device()->CreateUnorderedAccessView(
                m_D3D12Resource,
                nullptr,
                &d3d12UavDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
            );
        }

        return descriptor;
    }

    const Descriptor& Texture::GetRtv(const TextureRtv& textureRtv) const
    {
        BenzinAssert(m_D3D12Resource);
        BenzinAssert(textureRtv.DepthRange.Count < m_Depth);

        auto validatedRtv = textureRtv;
        validatedRtv.Format = textureRtv.Format != GraphicsFormat::Unknown ? textureRtv.Format : m_Format;
        validatedRtv.DepthRange.Count = textureRtv.DepthRange.Count != 0 ? textureRtv.DepthRange.Count : m_Depth;

        auto& descriptor = GetViewDescriptor(validatedRtv);
        if (!descriptor.IsCpuValid())
        {
            descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Rtv);

            const D3D12_RENDER_TARGET_VIEW_DESC d3d12RtvDesc = ToD3D12RenderTargetViewDesc(validatedRtv);

            m_Device.GetD3D12Device()->CreateRenderTargetView(
                m_D3D12Resource,
                &d3d12RtvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
            );
        }

        return descriptor;
    }

    const Descriptor& Texture::GetDsv() const
    {
        BenzinAssert(m_D3D12Resource);

        auto& descriptor = GetViewDescriptor(TextureDsv{});
        if (!descriptor.IsCpuValid())
        {
            descriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Dsv);

            m_Device.GetD3D12Device()->CreateDepthStencilView(
                m_D3D12Resource,
                nullptr, // Default D3D12_DEPTH_STENCIL_VIEW_DESC
                D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.GetCpuHandle() }
            );
        }
        
        return descriptor;
    }

} // namespace benzin

template <>
struct std::hash<benzin::TextureSrv>
{
    size_t operator()(const benzin::TextureSrv& textureSrv) const
    {
        static const auto baseHash = typeid(benzin::TextureSrv).hash_code();

        size_t hash = baseHash;
        hash = benzin::HashCombine(hash, textureSrv.Format);
        hash = benzin::HashCombine(hash, textureSrv.IsCubeMap);
        hash = benzin::HashCombine(hash, textureSrv.MostDetailedMipIndex);
        hash = benzin::HashCombine(hash, textureSrv.MipCount);
        hash = benzin::HashCombine(hash, textureSrv.DepthRange.StartIndex);
        hash = benzin::HashCombine(hash, textureSrv.DepthRange.Count);

        return hash;
    }
};

template <>
struct std::hash<benzin::TextureUav>
{
    size_t operator()(const benzin::TextureUav& textureUav) const
    {
        static const auto baseHash = typeid(benzin::TextureUav).hash_code();

        size_t hash = baseHash;
        hash = benzin::HashCombine(hash, textureUav.Format);
        hash = benzin::HashCombine(hash, textureUav.DepthRange.StartIndex);
        hash = benzin::HashCombine(hash, textureUav.DepthRange.Count);

        return hash;
    }
};

template <>
struct std::hash<benzin::TextureRtv>
{
    size_t operator()(const benzin::TextureRtv& textureRtv) const
    {
        static const auto baseHash = typeid(benzin::TextureRtv).hash_code();

        size_t hash = baseHash;
        hash = benzin::HashCombine(hash, textureRtv.Format);
        hash = benzin::HashCombine(hash, textureRtv.DepthRange.StartIndex);
        hash = benzin::HashCombine(hash, textureRtv.DepthRange.Count);

        return hash;
    }
};

template <>
struct std::hash<benzin::TextureDsv>
{
    size_t operator()(const benzin::TextureDsv& textureDsv) const
    {
        static const auto baseHash = typeid(benzin::TextureDsv).hash_code();
        return baseHash;
    }
};
