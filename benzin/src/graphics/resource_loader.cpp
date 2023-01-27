#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/resource_loader.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/graphics/graphics_command_list.hpp"

namespace benzin
{

    ResourceLoader::ResourceLoader(const Device& device)
        : m_Device{ device }
    {}

    std::shared_ptr<TextureResource> ResourceLoader::LoadTextureResourceFromDDSFile(const std::wstring_view& filepath, GraphicsCommandList& graphicsCommandList) const
    {
        BENZIN_ASSERT(m_Device.GetD3D12Device());
        BENZIN_ASSERT(!filepath.empty());

        std::vector<SubResourceData> subResources;

        // Create texture using Desc from DDS file
        bool isCubeMap = false;

        ID3D12Resource* d3d12Resource = nullptr;
        std::unique_ptr<uint8_t[]> textureData;

        BENZIN_D3D12_ASSERT(DirectX::LoadDDSTextureFromFile(
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

} // namespace benzin
