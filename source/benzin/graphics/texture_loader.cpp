#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/texture_loader.hpp"

#include <third_party/tinygltf/tiny_gltf.h>
#include <third_party/tinygltf/stb_image.h>

#include "benzin/graphics/api/command_queue.hpp"
#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/pipeline_state.hpp"
#include "benzin/graphics/api/texture.hpp"

#include "benzin/utility/string_utils.h"

namespace benzin
{

    namespace
    {

        const std::filesystem::path g_TexturesRootPath{ "../../assets/textures/" };

    } // anonymous namespece

    TextureLoader::TextureLoader(Device& device)
        : m_Device{ device }
    {
        const PipelineState::ComputeConfig config
        {
            .ComputeShader{ "equirectangular_to_cube.hlsl", "CS_Main" },
        };

        m_EquirectangularToCubePipelineState = std::make_unique<PipelineState>(m_Device, config);
        m_EquirectangularToCubePipelineState->SetDebugName("EquirectangularToCube");
    }

    TextureResource* TextureLoader::LoadFromDDSFile(std::string_view fileName) const
    {
        BENZIN_ASSERT(m_Device.GetD3D12Device());
        BENZIN_ASSERT(!fileName.empty());

        std::vector<SubResourceData> subResources;

        const std::filesystem::path filePath = g_TexturesRootPath / fileName;
        BENZIN_ASSERT(std::filesystem::exists(filePath));
        BENZIN_ASSERT(filePath.extension() == ".dds");

        ID3D12Resource* d3d12Resource = nullptr;
        std::unique_ptr<uint8_t[]> imageData;
        bool isCubeMap = false;

        const HRESULT hr = DirectX::LoadDDSTextureFromFile(
            m_Device.GetD3D12Device(),
            filePath.c_str(),
            &d3d12Resource,
            imageData,
            *reinterpret_cast<std::vector<D3D12_SUBRESOURCE_DATA>*>(&subResources),
            0,
            nullptr,
            &isCubeMap
        );

        if (FAILED(hr))
        {
            return nullptr;
        }

        auto* texture = new TextureResource{ m_Device, d3d12Resource };
        texture->m_Config.IsCubeMap = isCubeMap;

        {
            CommandQueueScope copyCommandQueue{ m_Device.GetCopyCommandQueue() };
            auto& copyCommandList = copyCommandQueue->GetCommandList(texture->GetSizeInBytes());
            copyCommandList.UpdateTexture(*texture, subResources);
        }

        return texture;
    }

    TextureResource* TextureLoader::LoadFromHDRFile(std::string_view fileName) const
    {
        BENZIN_ASSERT(m_Device.GetD3D12Device());
        BENZIN_ASSERT(!fileName.empty());

        const std::filesystem::path filePath = g_TexturesRootPath / fileName;
        BENZIN_ASSERT(std::filesystem::exists(filePath));
        BENZIN_ASSERT(filePath.extension() == ".hdr");

        int width;
        int height;
        const int componentCount = 4;
        float* imageData = stbi_loadf(ToNarrowString(filePath.c_str()).c_str(), &width, &height, nullptr, 4);

        if (!imageData)
        {
            return nullptr;
        }

        const TextureResource::Config config
        {
            .Type{ TextureResource::Type::Texture2D },
            .Width{ static_cast<uint32_t>(width) },
            .Height{ static_cast<uint32_t>(height) },
            .ArraySize{ 1 },
            .MipCount{ 1 },
            .Format{ GraphicsFormat::RGBA32Float },
        };

        auto* texture = new TextureResource{ m_Device, config };

        {
            CommandQueueScope copyCommandQueue{ m_Device.GetCopyCommandQueue() };
            auto& copyCommandList = copyCommandQueue->GetCommandList(texture->GetSizeInBytes());
            copyCommandList.UpdateTextureTopMip(*texture, reinterpret_cast<const std::byte*>(imageData));
        }

        stbi_image_free(imageData);

        return texture;
    }

    TextureResource* TextureLoader::LoadCubeMapFromHDRFile(std::string_view fileName, uint32_t size) const
    {
        auto equirectangularTexture = std::unique_ptr<TextureResource>{ LoadFromHDRFile(fileName) };
        equirectangularTexture->SetDebugName("Temp_HDRTexture");
        equirectangularTexture->PushShaderResourceView();
        
        if (!equirectangularTexture)
        {
            return nullptr;
        }

        const TextureResource::Config config
        {
            .Type{ TextureResource::Type::Texture2D },
            .Width{ size },
            .Height{ size },
            .ArraySize{ 6 },
            .MipCount{ 1 },
            .IsCubeMap{ true },
            .Format{ GraphicsFormat::RGBA32Float }, // TODO
            .Flags{ TextureResource::Flags::BindAsUnorderedAccess },
        };

        auto* cubeTexture = new TextureResource{ m_Device, config };
        cubeTexture->PushUnorderedAccessView({ .StartElementIndex{ 0 }, .ElementCount{ 6 } });

        ConvertEquirectangularToCube(*equirectangularTexture, *cubeTexture);

        return cubeTexture;
    }

    void TextureLoader::ConvertEquirectangularToCube(const TextureResource& equireactangularTexture, TextureResource& outCubeTexture) const
    {
        enum class RootConstant : uint32_t
        {
            InEquirectangularTextureIndex,
            OutCubeTextureIndex,
        };

        CommandQueueScope computeCommandQueue{ m_Device.GetComputeCommandQueue() };
        auto& computeCommandList = computeCommandQueue->GetCommandList();

        computeCommandList.SetPipelineState(*m_EquirectangularToCubePipelineState);

        computeCommandList.SetResourceBarrier(outCubeTexture, Resource::State::UnorderedAccess);

        computeCommandList.SetRootShaderResource(RootConstant::InEquirectangularTextureIndex, equireactangularTexture.GetShaderResourceView());
        computeCommandList.SetRootUnorderedAccess(RootConstant::OutCubeTextureIndex, outCubeTexture.GetUnorderedAccessView());

        const uint32_t size = outCubeTexture.GetConfig().Width;
        const uint32_t arraySize = outCubeTexture.GetConfig().ArraySize;

        BENZIN_ASSERT(arraySize == 6);
        computeCommandList.Dispatch({ size, size, arraySize }, { 8, 8, 1 });

        computeCommandList.SetResourceBarrier(outCubeTexture, Resource::State::Present);
    }

} // namespace benzin
