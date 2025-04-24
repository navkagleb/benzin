#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/texture.hpp"

#include "benzin/graphics/common.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/d3d12_assert.hpp"

namespace benzin
{

    struct TextureDsv {};

    static void ValidateTextureSrv(const Texture& texture, TextureSrv& outTextureSrv)
    {
        // #TODO: Validation for 'TextureSrv::MipRange'

        BenzinAssert(texture.GetD3D12Resource() != nullptr);
        BenzinAssert(outTextureSrv.DepthRange.Count <= texture.GetDepth());

        // Set default format for depth stencil if format is not set
        if (texture.GetAccessFlags().IsSet(TextureAccessFlag::AllowDepthStencil) && outTextureSrv.Format == GraphicsFormat::Unknown)
        {
            outTextureSrv.Format = GraphicsFormat::D24Unorm_X8Typeless;
        }

        outTextureSrv.Format = outTextureSrv.Format != GraphicsFormat::Unknown ? outTextureSrv.Format : texture.GetFormat();
        outTextureSrv.IsCubeMap = outTextureSrv.IsCubeMap ? true : texture.IsCubeMap();
        outTextureSrv.DepthRange.Count = outTextureSrv.DepthRange.Count != 0 ? outTextureSrv.DepthRange.Count : texture.GetDepth();
    }

    static void ValidateTextureUav(const Texture& texture, TextureUav& outTextureUav)
    {
        BenzinAssert(texture.GetD3D12Resource() != nullptr);
        BenzinAssert(texture.GetAccessFlags().IsSet(TextureAccessFlag::AllowUnorderedAccess));
        BenzinAssert(outTextureUav.MipIndex < texture.GetMipCount());
        BenzinAssert(outTextureUav.DepthRange.Count < texture.GetDepth()); // TODO: <= ?

        outTextureUav.Format = outTextureUav.Format != GraphicsFormat::Unknown ? outTextureUav.Format : texture.GetFormat();
        outTextureUav.DepthRange.Count = outTextureUav.DepthRange.Count != 0 ? outTextureUav.DepthRange.Count : texture.GetDepth();
    }

    static void ValidateTextureRtv(const Texture& texture, TextureRtv& outTextureRtv)
    {
        BenzinAssert(texture.GetD3D12Resource() != nullptr);
        BenzinAssert(texture.GetAccessFlags().IsSet(TextureAccessFlag::AllowRenderTarget));
        BenzinAssert(outTextureRtv.DepthRange.Count < texture.GetDepth());

        outTextureRtv.Format = outTextureRtv.Format != GraphicsFormat::Unknown ? outTextureRtv.Format : texture.GetFormat();
        outTextureRtv.DepthRange.Count = outTextureRtv.DepthRange.Count != 0 ? outTextureRtv.DepthRange.Count : texture.GetDepth();
    }

    static void ValidateTextureDsv(const Texture& texture)
    {
        BenzinUnused(texture);

        // A stopgap for future implementation

        BenzinAssert(texture.GetD3D12Resource() != nullptr);
        BenzinAssert(texture.GetAccessFlags().IsSet(TextureAccessFlag::AllowDepthStencil));
    }

    static D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const TextureCreation& textureCreation)
    {
        D3D12_RESOURCE_FLAGS d3d12ResourceFlags = D3D12_RESOURCE_FLAG_NONE;

        if (textureCreation.AccessFlags.IsSet(TextureAccessFlag::AllowRenderTarget))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }

        if (textureCreation.AccessFlags.IsSet(TextureAccessFlag::AllowDepthStencil))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        if (textureCreation.AccessFlags.IsSet(TextureAccessFlag::AllowUnorderedAccess))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        return D3D12_RESOURCE_DESC
        {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D, // For now only 2D textures supported
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

    static D3D12_CLEAR_VALUE ToD3D12ClearValue(const TextureCreation& textureCreation)
    {
        D3D12_CLEAR_VALUE d3d12ClearValue
        {
            .Format = (DXGI_FORMAT)textureCreation.Format
        };

        if (std::holds_alternative<std::monostate>(textureCreation.ClearValueVariant))
        {
            if (textureCreation.AccessFlags.IsSet(TextureAccessFlag::AllowRenderTarget))
            {
                const_cast<ClearValueVariant&>(textureCreation.ClearValueVariant) = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };
            }
            else if (textureCreation.AccessFlags.IsSet(TextureAccessFlag::AllowDepthStencil))
            {
                const_cast<ClearValueVariant&>(textureCreation.ClearValueVariant) = DepthStencilValue{};
            }
        }

        textureCreation.ClearValueVariant | MakeVisitorMatch(
            [&textureCreation, &d3d12ClearValue](const DirectX::XMFLOAT4& clearColor)
            {
                BenzinAssert(textureCreation.AccessFlags.IsSet(TextureAccessFlag::AllowRenderTarget));

                d3d12ClearValue.Color[0] = clearColor.x;
                d3d12ClearValue.Color[1] = clearColor.y;
                d3d12ClearValue.Color[2] = clearColor.z;
                d3d12ClearValue.Color[3] = clearColor.w;
            },
            [&textureCreation, &d3d12ClearValue](const DepthStencilValue& depthStencil)
            {
                BenzinAssert(textureCreation.AccessFlags.IsSet(TextureAccessFlag::AllowDepthStencil));

                d3d12ClearValue.DepthStencil.Depth = depthStencil.Depth;
                d3d12ClearValue.DepthStencil.Stencil = depthStencil.Stencil;
            },
            [](std::monostate)
            {
                BenzinEnsure(false);
            }
        );

        return d3d12ClearValue;
    }

    static void CreateD3D12Resource(
        const TextureCreation& textureCreation,
        const Device& device,
        ID3D12Resource*& outD3D12Resource,
        ResourceState& outInitialState
    )
    {
        BenzinAssert(textureCreation.Format != GraphicsFormat::Unknown);

        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(textureCreation);

        outInitialState = ResourceState::Common;

        if (textureCreation.AccessFlags.IsAnySet(TextureAccessFlag::AllowRenderTarget | TextureAccessFlag::AllowDepthStencil))
        {
            const D3D12_CLEAR_VALUE d3d12ClearValue = ToD3D12ClearValue(textureCreation);

            BenzinD3D12Call(device.GetD3D12Device()->CreateCommittedResource(
                &d3d12HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &d3d12ResourceDesc,
                (D3D12_RESOURCE_STATES)outInitialState,
                &d3d12ClearValue,
                IID_PPV_ARGS(&outD3D12Resource)
            ));
        }
        else
        {
            BenzinD3D12Call(device.GetD3D12Device()->CreateCommittedResource(
                &d3d12HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &d3d12ResourceDesc,
                (D3D12_RESOURCE_STATES)outInitialState,
                nullptr,
                IID_PPV_ARGS(&outD3D12Resource)
            ));
        }

        BenzinEnsure(outD3D12Resource != nullptr);
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResourceViewDesc(const Texture& texture, const TextureSrv& textureSrv)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc
        {
            .Format = (DXGI_FORMAT)textureSrv.Format,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        };

        const auto mipCount = GetGoodUintOr(textureSrv.MipRange.Count, g_Bad32);

        const bool isArrayTexture = texture.GetDepth() > 1;;
        if (!isArrayTexture)
        {
            d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            d3d12SrvDesc.Texture2D = D3D12_TEX2D_SRV
            {
                .MostDetailedMip = textureSrv.MipRange.StartIndex,
                .MipLevels = mipCount,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0.0f,
            };
        }
        else if (textureSrv.IsCubeMap)
        {
            d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            d3d12SrvDesc.TextureCube = D3D12_TEXCUBE_SRV
            {
                .MostDetailedMip = textureSrv.MipRange.StartIndex,
                .MipLevels = mipCount,
                .ResourceMinLODClamp = 0.0f,
            };
        }
        else
        {
            d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            d3d12SrvDesc.Texture2DArray = D3D12_TEX2D_ARRAY_SRV
            {
                .MostDetailedMip = textureSrv.MipRange.StartIndex,
                .MipLevels = mipCount,
                .FirstArraySlice = textureSrv.DepthRange.StartIndex,
                .ArraySize = textureSrv.DepthRange.Count,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0.0f,
            };
        }

        return d3d12SrvDesc;
    }

    static D3D12_UNORDERED_ACCESS_VIEW_DESC ToD3D12UnorderedAccessViewDesc(const Texture& texture, const TextureUav& textureUav)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc{ .Format = (DXGI_FORMAT)textureUav.Format };

        const bool isArrayTexture = texture.GetDepth() > 1;
        if (!isArrayTexture)
        {
            d3d12UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            d3d12UavDesc.Texture2D = D3D12_TEX2D_UAV
            {
                .MipSlice = textureUav.MipIndex,
                .PlaneSlice = 0,
            };
        }
        else
        {
            d3d12UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            d3d12UavDesc.Texture2DArray = D3D12_TEX2D_ARRAY_UAV
            {
                .MipSlice = textureUav.MipIndex,
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

    static DirectX::XMUINT3 GetMipDimensions(const DirectX::XMUINT3& sourceDimensions, uint16_t mipIndex)
    {
        return DirectX::XMUINT3
        {
            std::max(1u, sourceDimensions.x >> mipIndex),
            std::max(1u, sourceDimensions.y >> mipIndex),
            std::max(1u, sourceDimensions.z >> mipIndex),
        };
    }

    //

    Texture::Texture(Device& device, const TextureCreation& creation)
        : Resource{ device }
    {
        CreateD3D12Resource(creation, m_Device, m_D3D12Resource, m_CurrentState);
        SetD3DObjectDebugName(m_D3D12Resource, std::format("Texture_{}", creation.DebugName));

        m_IsCubeMap = creation.IsCubeMap;
        m_Format = creation.Format;
        m_Width = creation.Width;
        m_Height = creation.Height;
        m_Depth = creation.Depth;
        m_MipCount = creation.MipCount;
        m_AccessFlags = creation.AccessFlags;
        m_ClearValueVariant = creation.ClearValueVariant;
    }

    Texture::Texture(Device& device, ID3D12Resource* d3d12Resource)
        : Resource{ device }
    {
        BenzinAssert(d3d12Resource != nullptr);
        m_D3D12Resource = d3d12Resource;

        {
            const D3D12_RESOURCE_DESC d3d12ResourceDesc = d3d12Resource->GetDesc();
            m_Format = (GraphicsFormat)d3d12ResourceDesc.Format;
            m_Width = (uint32_t)d3d12ResourceDesc.Width;
            m_Height = d3d12ResourceDesc.Height;
            m_Depth = d3d12ResourceDesc.DepthOrArraySize;
            m_MipCount = d3d12ResourceDesc.MipLevels;

            if (d3d12ResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
            {
                m_AccessFlags.Set(TextureAccessFlag::AllowRenderTarget);
            }

            if (d3d12ResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            {
                m_AccessFlags.Set(TextureAccessFlag::AllowDepthStencil);
            }

            if (d3d12ResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
            {
                m_AccessFlags.Set(TextureAccessFlag::AllowUnorderedAccess);
            }
        }
    }

    const DirectX::XMFLOAT4& Texture::GetClearColor() const
    {
        BenzinAssert(std::holds_alternative<DirectX::XMFLOAT4>(m_ClearValueVariant));
        return std::get<DirectX::XMFLOAT4>(m_ClearValueVariant);
    }

    DepthStencilValue Texture::GetClearDepthStencil() const
    {
        BenzinAssert(std::holds_alternative<DepthStencilValue>(m_ClearValueVariant));
        return std::get<DepthStencilValue>(m_ClearValueVariant);
    }

    uint64_t Texture::GetSizeInBytes() const
    {
        BenzinAssert(m_D3D12Resource != nullptr);

        const D3D12_RESOURCE_DESC d3d12ResourceDesc = m_D3D12Resource->GetDesc();
        const uint32_t subResourceCount = GetSubResourceCount();

        uint64_t sizeInBytes = 0;
        m_Device.GetD3D12Device()->GetCopyableFootprints(
            &d3d12ResourceDesc,
            0, // first sub-resource
            subResourceCount,
            0,
            nullptr,
            nullptr,
            nullptr,
            &sizeInBytes
        );

        return sizeInBytes;
    }

    uint32_t Texture::GetSubResourceCount() const
    {
        return m_MipCount * m_Depth * m_Device.GetPlaneCountFromFormat(m_Format);
    }

    uint32_t Texture::GetMipWidth(uint16_t mipIndex) const
    {
        return GetMipDimensions({ m_Width, m_Height, m_Depth }, mipIndex).x;
    }

    uint32_t Texture::GetMipHeight(uint16_t mipIndex) const
    {
        return GetMipDimensions({ m_Width, m_Height, m_Depth }, mipIndex).y;
    }

    uint32_t Texture::CalcSubResourceIndex(uint16_t mipIndex, uint16_t depthIndex) const
    {
        // Ref: https://github.com/microsoft/DirectX-Graphics-Samples/blob/096d935f7f4a420cf96ecd6a010dce82f794e448/Libraries/D3D12RaytracingFallback/Include/d3dx12.h#L1684C13-L1684C33
        // TODO: Add plande slice index support

        return mipIndex + (depthIndex * m_MipCount);
    }

    const Descriptor& Texture::GetSrv(const TextureSrv& textureSrv) const
    {
        ValidateTextureSrv(*this, const_cast<TextureSrv&>(textureSrv));

        return TryGetViewDescriptor(
            GetStdHash(textureSrv),
            [&] { return CreateDetachedSrv(textureSrv, false); }
        );
    }

    const Descriptor& Texture::GetUav(const TextureUav& textureUav) const
    {
        ValidateTextureUav(*this, const_cast<TextureUav&>(textureUav));

        return TryGetViewDescriptor(
            GetStdHash(textureUav),
            [&] { return CreateDetachedUav(textureUav, false); }
        );
    }

    const Descriptor& Texture::GetRtv(const TextureRtv& textureRtv) const
    {
        ValidateTextureRtv(*this, const_cast<TextureRtv&>(textureRtv));

        return TryGetViewDescriptor(
            GetStdHash(textureRtv),
            [&] { return CreateDetachedRtv(textureRtv, false); }
        );
    }

    const Descriptor& Texture::GetDsv() const
    {
        ValidateTextureDsv(*this);

        return TryGetViewDescriptor(
            GetStdHash(TextureDsv{}),
            [&] { return CreateDetachedDsv(false); }
        );
    }

    Descriptor Texture::CreateDetachedSrv(const TextureSrv& textureSrv, bool isValidationEnabled) const
    {
        if (isValidationEnabled)
        {
            ValidateTextureSrv(*this, const_cast<TextureSrv&>(textureSrv));
        }

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Srv, [this, &textureSrv](uint64_t cpuHandle)
        {
            const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = ToD3D12ShaderResourceViewDesc(*this, textureSrv);

            m_Device.GetD3D12Device()->CreateShaderResourceView(
                m_D3D12Resource,
                &d3d12SrvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle }
            );
        });
    }

    Descriptor Texture::CreateDetachedUav(const TextureUav& textureUav, bool isValidationEnabled) const
    {
        if (isValidationEnabled)
        {
            ValidateTextureUav(*this, const_cast<TextureUav&>(textureUav));
        }

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Uav, [this, &textureUav](uint64_t cpuHandle)
        {
            const D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc = ToD3D12UnorderedAccessViewDesc(*this, textureUav);

            m_Device.GetD3D12Device()->CreateUnorderedAccessView(
                m_D3D12Resource,
                nullptr,
                &d3d12UavDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle }
            );
        });
    }

    Descriptor Texture::CreateDetachedRtv(const TextureRtv& textureRtv, bool isValidationEnabled) const
    {
        if (isValidationEnabled)
        {
            ValidateTextureRtv(*this, const_cast<TextureRtv&>(textureRtv));
        }

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Rtv, [&](uint64_t cpuHandle)
        {
            const D3D12_RENDER_TARGET_VIEW_DESC d3d12RtvDesc = ToD3D12RenderTargetViewDesc(textureRtv);

            m_Device.GetD3D12Device()->CreateRenderTargetView(
                m_D3D12Resource,
                &d3d12RtvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle }
            );
        });
    }

    Descriptor Texture::CreateDetachedDsv(bool isValidationEnabled) const
    {
        if (isValidationEnabled)
        {
            ValidateTextureDsv(*this);
        }

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Dsv, [&](uint64_t cpuHandle)
        {
            m_Device.GetD3D12Device()->CreateDepthStencilView(
                m_D3D12Resource,
                nullptr, // Default D3D12_DEPTH_STENCIL_VIEW_DESC
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle }
            );
        });
    }

}

BenzinDefineStdHashForType(benzin::TextureSrv, textureSrv,
{
    size_t hash = typeid(benzin::TextureSrv).hash_code();
    hash = benzin::HashCombine(hash, textureSrv.Format);
    hash = benzin::HashCombine(hash, textureSrv.IsCubeMap);
    hash = benzin::HashCombine(hash, textureSrv.DepthRange.StartIndex);
    hash = benzin::HashCombine(hash, textureSrv.DepthRange.Count);
    hash = benzin::HashCombine(hash, textureSrv.MipRange.StartIndex);
    hash = benzin::HashCombine(hash, textureSrv.MipRange.Count);

    return hash;
});

BenzinDefineStdHashForType(benzin::TextureUav, textureUav,
{
    size_t hash = typeid(benzin::TextureUav).hash_code();
    hash = benzin::HashCombine(hash, textureUav.Format);
    hash = benzin::HashCombine(hash, textureUav.MipIndex);
    hash = benzin::HashCombine(hash, textureUav.DepthRange.StartIndex);
    hash = benzin::HashCombine(hash, textureUav.DepthRange.Count);

    return hash;
});

BenzinDefineStdHashForType(benzin::TextureRtv, textureRtv,
{
    size_t hash = typeid(benzin::TextureRtv).hash_code();
    hash = benzin::HashCombine(hash, textureRtv.Format);
    hash = benzin::HashCombine(hash, textureRtv.DepthRange.StartIndex);
    hash = benzin::HashCombine(hash, textureRtv.DepthRange.Count);

    return hash;
});

BenzinDefineStdHashForType(benzin::TextureDsv, textureDsv,
{
    return typeid(benzin::TextureDsv).hash_code();
});
