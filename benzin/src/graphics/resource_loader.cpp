#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/resource_loader.hpp"

#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/texture.hpp"
#include "benzin/graphics/api/graphics_command_list.hpp"

namespace benzin
{

    namespace
    {

        std::string ConvertWideStringToString(const std::wstring& wideString)
        {
            if (wideString.empty())
            {
                return "";
            }

            const auto size = ::WideCharToMultiByte(
                CP_UTF8,
                0,
                wideString.c_str(),
                static_cast<int>(wideString.size()),
                nullptr,
                0,
                nullptr,
                nullptr
            );

            BENZIN_ASSERT(size);

            std::string result;
            result.resize(size);

            ::WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), static_cast<int>(wideString.size()), result.data(), size, nullptr, nullptr);
            
            return result;
        }

        std::string GetFilenameFromFilepath(std::wstring_view filepath)
        {
            const size_t beginFilenameIndex = filepath.find_last_of('/') + 1;
            const size_t endFilenameIndex = filepath.find_last_of('.');
            const size_t filenameSize = endFilenameIndex - beginFilenameIndex;


            const std::wstring_view wideFilenameView = filepath.substr(beginFilenameIndex, filenameSize);
            const std::wstring filename{ wideFilenameView.begin(), wideFilenameView.end() };

            return ConvertWideStringToString(filename);
        }

    } // anonymous namespece

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

        auto textureResource = m_Device.RegisterTextureResource(d3d12Resource, GetFilenameFromFilepath(filepath));
        textureResource->m_Config.IsCubeMap = isCubeMap;

        graphicsCommandList.UploadToTexture(*textureResource, subResources);

        return textureResource;
    }

} // namespace benzin
