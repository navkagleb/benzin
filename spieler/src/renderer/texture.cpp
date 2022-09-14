#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/texture.hpp"

#include "spieler/renderer/device.hpp"

#include "platform/dx12/dx12_common.hpp"

namespace spieler::renderer
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
                .Depth{ dx12Desc.DepthOrArraySize },
                .MipCount{ dx12Desc.MipLevels },
                .Format{ dx12::Convert(dx12Desc.Format) },
                .UsageFlags{ dx12::Convert(dx12Desc.Flags) }
            };
        }

    } // namespace _internal

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

    TextureResource::TextureResource(ComPtr<ID3D12Resource>&& dx12Texture)
        : Resource{ std::move(dx12Texture) }
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
            m_Config.Type = Type::CubeMap;
        }

        return subresources;
    }

} // namespace spieler::renderer
