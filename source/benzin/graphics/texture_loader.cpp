#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/texture_loader.hpp"

#include <third_party/tinygltf/tiny_gltf.h>
#include <third_party/tinygltf/stb_image.h>

#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/pipeline_state.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    TextureLoader::TextureLoader(Device& device)
        : m_Device{ device }
    {
        m_EquirectangularToCubePipelineState = std::make_unique<PipelineState>(m_Device, ComputePipelineStateCreation
        {
            .DebugName = "EquirectangularToCube",
            .ComputeShader{ "equirectangular_to_cube.hlsl", "CS_Main" },
        });
    }

    Texture* TextureLoader::LoadFromDDSFile(std::string_view fileName) const
    {
        const std::filesystem::path filePath = config::g_TextureDirPath / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".dds");

        ID3D12Resource* d3d12Resource = nullptr;
        std::unique_ptr<uint8_t[]> imageData;
        std::vector<SubResourceData> subResources;
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

        SetD3D12ObjectDebugName(d3d12Resource, fileName);

        auto* texture = new Texture{ m_Device, d3d12Resource };
        texture->m_IsCubeMap = isCubeMap;

        {
            auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
            BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

            auto& copyCommandList = copyCommandQueue.GetCommandList(texture->GetSizeInBytes());
            copyCommandList.UpdateTexture(*texture, subResources);
        }

        return texture;
    }

    Texture* TextureLoader::LoadFromHDRFile(std::string_view fileName) const
    {
        const std::filesystem::path filePath = config::g_TextureDirPath / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".hdr");

        int width;
        int height;
        const int componentCount = 4;
        float* imageData = stbi_loadf(filePath.string().c_str(), &width, &height, nullptr, 4);

        if (!imageData)
        {
            return nullptr;
        }

        auto* texture = new Texture
        {
            m_Device,
            TextureCreation
            {
                .DebugName = fileName,
                .Type = TextureType::Texture2D,
                .Format = GraphicsFormat::RGBA32Float,
                .Width = static_cast<uint32_t>(width),
                .Height = static_cast<uint32_t>(height),
                .MipCount = 1,
            }
        };

        {
            auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
            BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

            auto& copyCommandList = copyCommandQueue.GetCommandList(texture->GetSizeInBytes());
            copyCommandList.UpdateTextureTopMip(*texture, reinterpret_cast<const std::byte*>(imageData));
        }

        stbi_image_free(imageData);

        return texture;
    }

    Texture* TextureLoader::LoadCubeMapFromHDRFile(std::string_view fileName, uint32_t size) const
    {
        auto equirectangularTexture = std::unique_ptr<Texture>{ LoadFromHDRFile(fileName) };
        equirectangularTexture->PushShaderResourceView();
        
        if (!equirectangularTexture)
        {
            return nullptr;
        }

        auto* cubeTexture = new Texture
        {
            m_Device,
            TextureCreation
            {
                .DebugName = fileName,
                .Type = TextureType::Texture2D,
                .IsCubeMap = true,
                .Format = GraphicsFormat::RGBA32Float,
                .Width = size,
                .Height = size,
                .ArraySize = 6,
                .MipCount = 1,
                .Flags = TextureFlag::AllowUnorderedAccess,
                .IsNeedUnorderedAccessView = true,
            }
        };

        ToEquirectangularToCube(*equirectangularTexture, *cubeTexture);

        return cubeTexture;
    }

    void TextureLoader::ToEquirectangularToCube(const Texture& equireactangularTexture, Texture& outCubeTexture) const
    {
        enum class RootConstant : uint32_t
        {
            InEquirectangularTextureIndex,
            OutCubeTextureIndex,
        };

        auto& computeCommandQueue = m_Device.GetComputeCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(computeCommandQueue);

        auto& computeCommandList = computeCommandQueue.GetCommandList();

        computeCommandList.SetPipelineState(*m_EquirectangularToCubePipelineState);

        computeCommandList.SetRootShaderResource(RootConstant::InEquirectangularTextureIndex, equireactangularTexture.GetShaderResourceView());
        computeCommandList.SetRootUnorderedAccess(RootConstant::OutCubeTextureIndex, outCubeTexture.GetUnorderedAccessView());

        computeCommandList.SetResourceBarrier(TransitionBarrier{ outCubeTexture, ResourceState::UnorderedAccess });

        const uint32_t size = outCubeTexture.GetWidth();
        const uint32_t arraySize = outCubeTexture.GetArraySize();
        BenzinAssert(arraySize == 6);
        computeCommandList.Dispatch({ size, size, arraySize }, { 8, 8, 1 });

        computeCommandList.SetResourceBarrier(TransitionBarrier{ outCubeTexture, ResourceState::Present });
    }

} // namespace benzin
