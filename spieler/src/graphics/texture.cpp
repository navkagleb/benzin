#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/texture.hpp"

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/graphics_command_list.hpp"

#include "platform/dx12/dx12_common.hpp"

namespace spieler
{

    namespace _internal
    {

        TextureResource::Config ConvertFromDX12ResourceDesc(const D3D12_RESOURCE_DESC& dx12Desc)
        {
            return TextureResource::Config
            {
                .Type{ dx12::Convert(dx12Desc.Dimension) },
                .Width{ static_cast<uint32_t>(dx12Desc.Width) },
                .Height{ dx12Desc.Height },
                .ArraySize{ dx12Desc.DepthOrArraySize },
                .MipCount{ dx12Desc.MipLevels },
                .Format{ dx12::Convert(dx12Desc.Format) },
                .UsageFlags{ dx12::Convert(dx12Desc.Flags) }
            };
        }

    } // namespace _internal

    //////////////////////////////////////////////////////////////////////////
    /// TextureResource
    //////////////////////////////////////////////////////////////////////////
    std::shared_ptr<TextureResource> TextureResource::Create(Device& device, const Config& config)
    {
        return std::make_shared<TextureResource>(device, config);
    }

    std::shared_ptr<TextureResource> TextureResource::Create(Device& device, const Config& config, const ClearColor& clearColor)
    {
        return std::make_shared<TextureResource>(device, config, clearColor);
    }

    std::shared_ptr<TextureResource> TextureResource::Create(Device& device, const Config& config, const ClearDepthStencil& clearDepthStencil)
    {
        return std::make_shared<TextureResource>(device, config, clearDepthStencil);
    }

    std::shared_ptr<TextureResource> TextureResource::Create(ID3D12Resource* dx12Resource)
    {
        return std::make_shared<TextureResource>(dx12Resource);
    }

    std::shared_ptr<TextureResource> TextureResource::LoadFromDDSFile(Device& device, GraphicsCommandList& graphicsCommandList, const std::wstring& filename)
    {
        SPIELER_ASSERT(device.GetDX12Device());
        SPIELER_ASSERT(!filename.empty());
        
        std::shared_ptr<TextureResource> textureResource{ std::make_shared<TextureResource>() };
        std::vector<SubresourceData> subresources;

        // Create texture using Desc from DDS file
        bool isCubeMap{ false };

        SPIELER_ASSERT(SUCCEEDED(DirectX::LoadDDSTextureFromFile(
            device.GetDX12Device(),
            filename.c_str(),
            &textureResource->m_DX12Resource,
            textureResource->m_Data,
            *reinterpret_cast<std::vector<D3D12_SUBRESOURCE_DATA>*>(&subresources),
            0, // maxSize default value is 0, so leave it
            nullptr,
            &isCubeMap
        )));

        textureResource->m_Config = _internal::ConvertFromDX12ResourceDesc(textureResource->m_DX12Resource->GetDesc());

        if (isCubeMap)
        {
            textureResource->m_Config.Type = Type::Cube;
        }

        graphicsCommandList.UploadToTexture(*textureResource, subresources);

        return textureResource;
    }

    TextureResource::TextureResource(Device& device, const Config& config)
        : Resource{ device.CreateDX12Texture(config) }
        , m_Config{ config }
    {}

    TextureResource::TextureResource(Device& device, const Config& config, const ClearColor& clearColor)
        : Resource{ device.CreateDX12Texture(config, clearColor) }
        , m_Config{ config }
    {}

    TextureResource::TextureResource(Device& device, const Config& config, const ClearDepthStencil& clearDepthStencil)
        : Resource{ device.CreateDX12Texture(config, clearDepthStencil) }
        , m_Config{ config }
    {}

    TextureResource::TextureResource(ID3D12Resource* dx12Resource)
        : Resource{ dx12Resource }
        , m_Config{ _internal::ConvertFromDX12ResourceDesc(m_DX12Resource->GetDesc()) }
    {}

    std::vector<SubresourceData> TextureResource::LoadFromDDSFile(Device& device, const std::wstring& filename)
    {
        SPIELER_ASSERT(device.GetDX12Device());
        SPIELER_ASSERT(!filename.empty());

        std::vector<SubresourceData> subresources;

        // Create texture using Desc from DDS file
        bool isCubeMap{ false };

        SPIELER_ASSERT(SUCCEEDED(DirectX::LoadDDSTextureFromFile(
            device.GetDX12Device(),
            filename.c_str(),
            &m_DX12Resource,
            m_Data,
            *reinterpret_cast<std::vector<D3D12_SUBRESOURCE_DATA>*>(&subresources),
            0, // maxSize default value is 0, so leave it
            nullptr,
            &isCubeMap
        )));

        m_Config = _internal::ConvertFromDX12ResourceDesc(m_DX12Resource->GetDesc());
        
        if (isCubeMap)
        {
            m_Config.Type = Type::Cube;
        }

        return subresources;
    }

    //////////////////////////////////////////////////////////////////////////
    /// TextureShaderResourceView
    //////////////////////////////////////////////////////////////////////////
    TextureShaderResourceView::TextureShaderResourceView(Device& device, const TextureResource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateSRV();

        const D3D12_RESOURCE_DESC dx12ResourceDesc{ resource.GetDX12Resource()->GetDesc() };

        D3D12_SHADER_RESOURCE_VIEW_DESC dx12SRVDesc
        {
            .Format{ dx12ResourceDesc.Format },
            .Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING }
        };

        switch (resource.GetConfig().Type)
        {
            case TextureResource::Type::_2D:
            {
                // TODO: adapt this method for array of Texture::2D
                
                dx12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                dx12SRVDesc.Texture2D = D3D12_TEX2D_SRV
                {
                    .MostDetailedMip{ 0 },
                    .MipLevels{ dx12ResourceDesc.MipLevels },
                    .PlaneSlice{ 0 },
                    .ResourceMinLODClamp{ 0.0f }
                };

                break;
            }
            case TextureResource::Type::Cube:
            {
                dx12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                dx12SRVDesc.TextureCube = D3D12_TEXCUBE_SRV
                {
                    .MostDetailedMip{ 0 },
                    .MipLevels{ dx12ResourceDesc.MipLevels },
                    .ResourceMinLODClamp{ 0.0f }
                };

                break;
            }
            default:
            {
                SPIELER_ASSERT(false);
            }
        }

        device.GetDX12Device()->CreateShaderResourceView(
            resource.GetDX12Resource(),
            &dx12SRVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    TextureShaderResourceView::TextureShaderResourceView(Device& device, const TextureResource& resource, const Config& config)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());
        SPIELER_ASSERT(resource.GetConfig().Format != GraphicsFormat::Unknown);

        SPIELER_ASSERT(config.Type != TextureResource::Type::Unknown);

        //SPIELER_ASSERT(config.MostDetailedMip + config.MipCount <= resource.GetConfig().MipCount);

        D3D12_SHADER_RESOURCE_VIEW_DESC dx12SRVDesc
        {
            .Format{ dx12::Convert(resource.GetConfig().Format) },
            .Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING }
        };

        switch (config.Type)
        {
            case TextureResource::Type::_2D:
            {
                if (resource.GetConfig().ArraySize == 1)
                {
                    dx12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    dx12SRVDesc.Texture2D = D3D12_TEX2D_SRV
                    {
                        .MostDetailedMip{ config.MostDetailedMipIndex },
                        .MipLevels{ config.MipCount },
                        .PlaneSlice{ 0 },
                        .ResourceMinLODClamp{ 0.0f }
                    };
                }
                else
                {
                    dx12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    dx12SRVDesc.Texture2DArray = D3D12_TEX2D_ARRAY_SRV
                    {
                        .MostDetailedMip{ config.MostDetailedMipIndex },
                        .MipLevels{ config.MipCount },
                        .FirstArraySlice{ config.FirstArrayElement },
                        .ArraySize{ config.ArraySize },
                        .PlaneSlice{ 0 },
                        .ResourceMinLODClamp{ 0.0f }
                    };
                }

                break;
            }
            case TextureResource::Type::Cube:
            {
                dx12SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                dx12SRVDesc.TextureCube = D3D12_TEXCUBE_SRV
                {
                    .MostDetailedMip{ config.MostDetailedMipIndex },
                    .MipLevels{ config.MipCount },
                    .ResourceMinLODClamp{ 0.0f }
                };

                break;
            }
            default:
            {
                SPIELER_ASSERT(false);
                break;
            }
        }

        m_Descriptor = device.GetDescriptorManager().AllocateSRV();

        device.GetDX12Device()->CreateShaderResourceView(
            resource.GetDX12Resource(),
            &dx12SRVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    //////////////////////////////////////////////////////////////////////////
    /// TextureUnorderedAccessView
    //////////////////////////////////////////////////////////////////////////
    TextureUnorderedAccessView::TextureUnorderedAccessView(Device& device, const TextureResource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateUAV();

        device.GetDX12Device()->CreateUnorderedAccessView(
            resource.GetDX12Resource(),
            nullptr,
            nullptr,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    //////////////////////////////////////////////////////////////////////////
    /// TextureRenderTargetView
    //////////////////////////////////////////////////////////////////////////
    TextureRenderTargetView::TextureRenderTargetView(Device& device, const TextureResource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateRTV();

        device.GetDX12Device()->CreateRenderTargetView(
            resource.GetDX12Resource(),
            nullptr,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    TextureRenderTargetView::TextureRenderTargetView(Device& device, const TextureResource& resource, const TextureViewConfig& config)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateRTV();

        D3D12_RENDER_TARGET_VIEW_DESC dx12RTVDesc
        {
            .Format{ config.Format != GraphicsFormat::Unknown ? dx12::Convert(config.Format) : dx12::Convert(resource.GetConfig().Format) }
        };

        switch (resource.GetConfig().Type)
        {
            case TextureResource::Type::_2D:
            {
                if (resource.GetConfig().ArraySize == 1)
                {
                    dx12RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                    dx12RTVDesc.Texture2D = D3D12_TEX2D_RTV
                    {
                        .MipSlice{ config.MostDetailedMip },
                        .PlaneSlice{ 0 }
                    };
                }
                else
                {
                    dx12RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    dx12RTVDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
                    {
                        .MipSlice{ config.MostDetailedMip },
                        .FirstArraySlice{ config.FirstArraySlice },
                        .ArraySize{ config.ArraySize },
                        .PlaneSlice{ 0 }
                    };
                }

                break;
            }
            default:
            {
                SPIELER_ASSERT(false);
                break;
            }
        }

        device.GetDX12Device()->CreateRenderTargetView(
            resource.GetDX12Resource(),
            &dx12RTVDesc,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    //////////////////////////////////////////////////////////////////////////
    /// TextureDepthStencilView
    //////////////////////////////////////////////////////////////////////////
    TextureDepthStencilView::TextureDepthStencilView(Device& device, const TextureResource& resource)
    {
        SPIELER_ASSERT(resource.GetDX12Resource());

        m_Descriptor = device.GetDescriptorManager().AllocateDSV();

        device.GetDX12Device()->CreateDepthStencilView(
            resource.GetDX12Resource(),
            nullptr,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_Descriptor.CPU }
        );
    }

    //////////////////////////////////////////////////////////////////////////
    /// Texture
    //////////////////////////////////////////////////////////////////////////
    Texture::Texture()
    {

    }

    void Texture::SetTextureResource(std::shared_ptr<TextureResource>&& textureResource)
    {
        m_TextureResource = std::move(textureResource);
        m_Views.clear();
    }

} // namespace spieler
