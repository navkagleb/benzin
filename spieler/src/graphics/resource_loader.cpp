#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/resource_loader.hpp"

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/texture.hpp"
#include "spieler/graphics/graphics_command_list.hpp"

namespace spieler
{

    ResourceLoader::ResourceLoader(const Device& device)
        : m_Device{ device }
    {}

    std::shared_ptr<TextureResource> ResourceLoader::LoadTextureResourceFromDDSFile(const std::wstring_view& filepath, GraphicsCommandList& graphicsCommandList) const
    {
        SPIELER_ASSERT(m_Device.GetD3D12Device());
        SPIELER_ASSERT(!filepath.empty());

        std::vector<SubResourceData> subResources;

        // Create texture using Desc from DDS file
        bool isCubeMap = false;

        ID3D12Resource* d3d12Resource = nullptr;
        std::unique_ptr<uint8_t[]> textureData;

        SPIELER_D3D12_ASSERT(DirectX::LoadDDSTextureFromFile(
            m_Device.GetD3D12Device(),
            filepath.data(),
            &d3d12Resource,
            textureData,
            *reinterpret_cast<std::vector<D3D12_SUBRESOURCE_DATA>*>(&subResources),
            0, // maxSize default value is 0, so leave it
            nullptr,
            &isCubeMap
        ));

        auto textureResource = m_Device.RegisterTextureResource(d3d12Resource);
        textureResource->m_Config.IsCubeMap = isCubeMap;

        graphicsCommandList.UploadToTexture(*textureResource, subResources);

        return textureResource;
    }

} // namespace spieler
