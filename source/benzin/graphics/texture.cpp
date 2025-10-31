#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/texture.hpp>

#include <benzin/graphics/common.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>

namespace benzin
{

    struct TextureRtv {};
    struct TextureDsv {};

    static D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const TextureCreation& textureCreation)
    {
        D3D12_RESOURCE_FLAGS d3d12ResourceFlags = D3D12_RESOURCE_FLAG_NONE;

        if (textureCreation.m_AccessFlags.IsSet(TextureAccessFlag::AllowRenderTarget))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }

        if (textureCreation.m_AccessFlags.IsSet(TextureAccessFlag::AllowDepthStencil))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        if (textureCreation.m_AccessFlags.IsSet(TextureAccessFlag::AllowUnorderedAccess))
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        D3D12_RESOURCE_DESC d3d12ResourceDesc = {};
        d3d12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // For now only 2D textures supported
        d3d12ResourceDesc.Alignment = 0;
        d3d12ResourceDesc.Width = (uint64_t)textureCreation.m_Width;
        d3d12ResourceDesc.Height = textureCreation.m_Height;
        d3d12ResourceDesc.DepthOrArraySize = textureCreation.m_Depth;
        d3d12ResourceDesc.MipLevels = textureCreation.m_MipCount;
        d3d12ResourceDesc.Format = textureCreation.m_DxgiFormat;
        d3d12ResourceDesc.SampleDesc = { 1, 0 };
        d3d12ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        d3d12ResourceDesc.Flags = d3d12ResourceFlags;

        return d3d12ResourceDesc;
    }

    static D3D12_CLEAR_VALUE ToD3D12ClearValue(const TextureCreation& creation)
    {
        D3D12_CLEAR_VALUE d3d12ClearValue = {};
        d3d12ClearValue.Format = creation.m_DxgiFormat;

        if (std::holds_alternative<std::monostate>(creation.m_ClearValueVariant))
        {
            if (creation.m_AccessFlags.IsSet(TextureAccessFlag::AllowRenderTarget))
            {
                const_cast<ClearValueVariant&>(creation.m_ClearValueVariant) = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };
            }
            else if (creation.m_AccessFlags.IsSet(TextureAccessFlag::AllowDepthStencil))
            {
                const_cast<ClearValueVariant&>(creation.m_ClearValueVariant) = DepthStencilValue{};
            }
        }

        creation.m_ClearValueVariant | MakeVisitorMatch(
            [&creation, &d3d12ClearValue](const DirectX::XMFLOAT4& clearColor)
            {
                BenzinAssert(creation.m_AccessFlags.IsSet(TextureAccessFlag::AllowRenderTarget));

                d3d12ClearValue.Color[0] = clearColor.x;
                d3d12ClearValue.Color[1] = clearColor.y;
                d3d12ClearValue.Color[2] = clearColor.z;
                d3d12ClearValue.Color[3] = clearColor.w;
            },
            [&creation, &d3d12ClearValue](const DepthStencilValue& depthStencil)
            {
                BenzinAssert(creation.m_AccessFlags.IsSet(TextureAccessFlag::AllowDepthStencil));

                d3d12ClearValue.DepthStencil.Depth = depthStencil.m_Depth;
                d3d12ClearValue.DepthStencil.Stencil = depthStencil.m_Stencil;
            },
            [](std::monostate)
            {
                BenzinEnsure(false);
            });

        return d3d12ClearValue;
    }

    static ID3D12Resource* CreateD3D12CommittedResource(const Device& device, const TextureCreation& creation, D3D12_RESOURCE_STATES d3d12InitialState)
    {
        BenzinAssert(creation.m_DxgiFormat != DXGI_FORMAT_UNKNOWN);

        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(creation);

        ID3D12Resource* d3d12Resource = nullptr;

        if (creation.m_AccessFlags.IsAnySet(TextureAccessFlag::AllowRenderTarget | TextureAccessFlag::AllowDepthStencil))
        {
            const D3D12_CLEAR_VALUE d3d12ClearValue = ToD3D12ClearValue(creation);

            BenzinD3D12Call(device.GetD3D12Device()->CreateCommittedResource(
                &d3d12HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &d3d12ResourceDesc,
                d3d12InitialState,
                &d3d12ClearValue,
                IID_PPV_ARGS(&d3d12Resource)));
        }
        else
        {
            BenzinD3D12Call(device.GetD3D12Device()->CreateCommittedResource(
                &d3d12HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &d3d12ResourceDesc,
                d3d12InitialState,
                nullptr,
                IID_PPV_ARGS(&d3d12Resource)));
        }

        BenzinEnsure(d3d12Resource != nullptr);
        return d3d12Resource;
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResourceViewDesc(const Texture& texture, const TextureSrv& textureSrv)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = {};
        d3d12SrvDesc.Format = textureSrv.m_DxgiFormat;
        d3d12SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        const bool isArrayTexture = texture.GetDepth() > 1;;
        if (!isArrayTexture)
        {
            d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            d3d12SrvDesc.Texture2D.MostDetailedMip = textureSrv.m_MipOffset;
            d3d12SrvDesc.Texture2D.MipLevels = textureSrv.m_MipCount;
            d3d12SrvDesc.Texture2D.PlaneSlice = 0;
            d3d12SrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        }
        else if (textureSrv.m_IsCubeMap)
        {
            d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            d3d12SrvDesc.TextureCube.MostDetailedMip = textureSrv.m_MipOffset;
            d3d12SrvDesc.TextureCube.MipLevels = textureSrv.m_MipCount;
            d3d12SrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        }
        else
        {
            d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            d3d12SrvDesc.Texture2DArray.MostDetailedMip = textureSrv.m_MipOffset;
            d3d12SrvDesc.Texture2DArray.MipLevels = textureSrv.m_MipCount;
            d3d12SrvDesc.Texture2DArray.FirstArraySlice = textureSrv.m_DepthOffset;
            d3d12SrvDesc.Texture2DArray.ArraySize = textureSrv.m_DepthCount;
            d3d12SrvDesc.Texture2DArray.PlaneSlice = 0;
            d3d12SrvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
        }

        return d3d12SrvDesc;
    }

    static D3D12_UNORDERED_ACCESS_VIEW_DESC ToD3D12UnorderedAccessViewDesc(const Texture& texture, const TextureUav& textureUav)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc = {};
        d3d12UavDesc.Format = texture.GetDxgiFormat();

        const bool isArrayTexture = texture.GetDepth() > 1;
        if (!isArrayTexture)
        {
            d3d12UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            d3d12UavDesc.Texture2D.MipSlice = textureUav.m_MipIndex;
            d3d12UavDesc.Texture2D.PlaneSlice = 0;
        }
        else
        {
            d3d12UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            d3d12UavDesc.Texture2DArray.MipSlice = textureUav.m_MipIndex;
            d3d12UavDesc.Texture2DArray.FirstArraySlice = 0;
            d3d12UavDesc.Texture2DArray.ArraySize = texture.GetDepth();
            d3d12UavDesc.Texture2DArray.PlaneSlice = 0;
        }

        return d3d12UavDesc;
    }

    static D3D12_RENDER_TARGET_VIEW_DESC ToD3D12RenderTargetViewDesc(const Texture& texture)
    {
        BenzinAssert(texture.GetDepth() == 1);

        D3D12_RENDER_TARGET_VIEW_DESC d3d12RtvDesc = {};
        d3d12RtvDesc.Format = texture.GetDxgiFormat();
        d3d12RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        d3d12RtvDesc.Texture2D.MipSlice = 0;
        d3d12RtvDesc.Texture2D.PlaneSlice = 0;

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
        m_D3D12CurrentState = D3D12_RESOURCE_STATE_COMMON;
        m_D3D12Resource = CreateD3D12CommittedResource(m_Device, creation, m_D3D12CurrentState);

        SetupCreation(&creation);
    }

    Texture::Texture(Device& device, ID3D12Resource* d3d12Resource)
        : Resource{ device }
    {
        BenzinAssert(d3d12Resource != nullptr);

        m_D3D12CurrentState = D3D12_RESOURCE_STATE_COMMON;
        m_D3D12Resource = d3d12Resource;

        SetupCreation();
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
        return m_MipCount * m_Depth * m_Device.GetPlaneCountFromFormat(m_DxgiFormat);
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
        return TryGetViewDescriptor(
            GetStdHash(textureSrv),
            [&] { return CreateDetachedSrv(const_cast<TextureSrv&>(textureSrv)); });
    }

    const Descriptor& Texture::GetUav(const TextureUav& textureUav) const
    {
        return TryGetViewDescriptor(
            GetStdHash(textureUav),
            [&] { return CreateDetachedUav(textureUav); });
    }

    const Descriptor& Texture::GetRtv() const
    {
        return TryGetViewDescriptor(
            GetStdHash(TextureRtv{}),
            [&] { return CreateDetachedRtv(); });
    }

    const Descriptor& Texture::GetDsv() const
    {
        return TryGetViewDescriptor(
            GetStdHash(TextureDsv{}),
            [&] { return CreateDetachedDsv(); });
    }

    Descriptor Texture::CreateDetachedSrv(TextureSrv& textureSrv) const
    {
        // Set default format for depth stencil if format is not set
        if (m_AccessFlags.IsSet(TextureAccessFlag::AllowDepthStencil) && textureSrv.m_DxgiFormat == DXGI_FORMAT_UNKNOWN)
        {
            BenzinAssert(m_DxgiFormat == DXGI_FORMAT_D24_UNORM_S8_UINT);
            textureSrv.m_DxgiFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        }

        textureSrv.m_IsCubeMap = textureSrv.m_IsCubeMap ? true : m_IsCubeMap;
        textureSrv.m_DxgiFormat = textureSrv.m_DxgiFormat != DXGI_FORMAT_UNKNOWN ? textureSrv.m_DxgiFormat : m_DxgiFormat;

        if (textureSrv.m_DepthOffset == 0 && IsMaxUint(textureSrv.m_DepthCount))
        {
            textureSrv.m_DepthCount = m_Depth;
        }

        if (textureSrv.m_MipOffset == 0 && IsMaxUint(textureSrv.m_MipCount))
        {
            textureSrv.m_MipCount = m_MipCount;
        }

        BenzinAssert(textureSrv.m_DepthOffset + textureSrv.m_DepthCount <= m_Depth);
        BenzinAssert(textureSrv.m_MipOffset + textureSrv.m_MipCount <= m_MipCount);

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Srv, [this, &textureSrv](uint64_t cpuHandle)
        {
            const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = ToD3D12ShaderResourceViewDesc(*this, textureSrv);

            m_Device.GetD3D12Device()->CreateShaderResourceView(
                m_D3D12Resource,
                &d3d12SrvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle });
        });
    }

    Descriptor Texture::CreateDetachedUav(const TextureUav& textureUav) const
    {
        BenzinAssert(m_D3D12CurrentState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        BenzinAssert(textureUav.m_MipIndex < m_MipCount);

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Uav, [this, &textureUav](uint64_t cpuHandle)
        {
            const D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc = ToD3D12UnorderedAccessViewDesc(*this, textureUav);

            m_Device.GetD3D12Device()->CreateUnorderedAccessView(
                m_D3D12Resource,
                nullptr,
                &d3d12UavDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle });
        });
    }

    Descriptor Texture::CreateDetachedRtv() const
    {
        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Rtv, [&](uint64_t cpuHandle)
        {
            const D3D12_RENDER_TARGET_VIEW_DESC d3d12RtvDesc = ToD3D12RenderTargetViewDesc(*this);

            m_Device.GetD3D12Device()->CreateRenderTargetView(
                m_D3D12Resource,
                &d3d12RtvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle });
        });
    }

    Descriptor Texture::CreateDetachedDsv() const
    {
        BenzinAssert(m_AccessFlags.IsSet(TextureAccessFlag::AllowDepthStencil));

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Dsv, [&](uint64_t cpuHandle)
        {
            m_Device.GetD3D12Device()->CreateDepthStencilView(
                m_D3D12Resource,
                nullptr, // Default D3D12_DEPTH_STENCIL_VIEW_DESC
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle });
        });
    }

    void Texture::SetupCreation(const TextureCreation* creation)
    {
        BenzinAssert(m_D3D12Resource != nullptr);

        const D3D12_RESOURCE_DESC d3d12ResourceDesc = m_D3D12Resource->GetDesc();

        if (creation != nullptr)
        {
            SetD3DObjectDebugName(m_D3D12Resource, creation->m_DebugName);

            m_IsCubeMap = creation->m_IsCubeMap;
            m_DxgiFormat = creation->m_DxgiFormat;
            m_Width = creation->m_Width;
            m_Height = creation->m_Height;
            m_Depth = creation->m_Depth;
            m_AccessFlags = creation->m_AccessFlags;
            m_ClearValueVariant = creation->m_ClearValueVariant;

            // NOTE: When zero MipCount is provided in TextureCreation than D3D12 creates full mip chain
            // Using that actual mip count can be retrieved through D3D12_RESOURCE_DESC
            m_MipCount = d3d12ResourceDesc.MipLevels;
        }
        else
        {
            m_DxgiFormat = d3d12ResourceDesc.Format;
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

}

BenzinDefineStdHashForType(benzin::TextureSrv, textureSrv,
{
    size_t hash = typeid(benzin::TextureSrv).hash_code();
    hash = benzin::HashCombine(hash, textureSrv.m_DxgiFormat);
    hash = benzin::HashCombine(hash, textureSrv.m_IsCubeMap);
    hash = benzin::HashCombine(hash, textureSrv.m_DepthOffset);
    hash = benzin::HashCombine(hash, textureSrv.m_DepthCount);
    hash = benzin::HashCombine(hash, textureSrv.m_MipOffset);
    hash = benzin::HashCombine(hash, textureSrv.m_MipCount);

    return hash;
});

BenzinDefineStdHashForType(benzin::TextureUav, textureUav,
{
    size_t hash = typeid(benzin::TextureUav).hash_code();
    hash = benzin::HashCombine(hash, textureUav.m_MipIndex);

    return hash;
});

BenzinDefineStdHashForType(benzin::TextureRtv, textureRtv,
{
    return typeid(benzin::TextureRtv).hash_code();
});

BenzinDefineStdHashForType(benzin::TextureDsv, textureDsv,
{
    return typeid(benzin::TextureDsv).hash_code();
});
