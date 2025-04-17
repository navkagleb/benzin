#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/resource_helper.hpp>

#include <DirectXTex.h>

namespace benzin
{

    bool SaveTextureArrayToDds(std::span<const std::string_view> fileNames, std::string_view outputFileName)
    {
        const uint32_t arraySize = (uint32_t)fileNames.size();

        std::vector<DirectX::ScratchImage> images;
        images.reserve(arraySize);

        for (const auto fileName : fileNames)
        {
            const std::filesystem::path filePath = EngineConfig::s_TextureDir / fileName;
            BenzinAssert(std::filesystem::exists(filePath));
            BenzinAssert(filePath.extension() == ".dds");

            DirectX::ScratchImage image;
            if (FAILED(DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)))
            {
                return false;
            }

            BenzinAssert(image.GetImageCount() == 1);
            BenzinAssert(image.GetMetadata().depth == 1);
            BenzinAssert(image.GetMetadata().arraySize == 1);

            images.push_back(std::move(image));
        }

        const size_t width = images[0].GetMetadata().width;
        const size_t height = images[0].GetMetadata().height;
        const DXGI_FORMAT format = images[0].GetMetadata().format;

#if BENZIN_IS_ASSERTS_ENABLED
        for (uint32_t i = 1; i < arraySize; ++i)
        {
            BenzinAssert(images[i].GetMetadata().width == width);
            BenzinAssert(images[i].GetMetadata().height == height);
            BenzinAssert(images[i].GetMetadata().format == format);
        }
#endif

        DirectX::ScratchImage textureArray;
        if (FAILED(textureArray.Initialize2D(format, width, height, arraySize, 1)))
        {
            return false;
        }

        for (uint32_t i = 0; i < arraySize; ++i)
        {
            const DirectX::Image* sourceImage = images[i].GetImage(0, 0, 0);
            const DirectX::Image* destImage = textureArray.GetImage(0, i, 0);

            BenzinAssert(sourceImage != nullptr && destImage != nullptr);
            memcpy(destImage->pixels, sourceImage->pixels, sourceImage->slicePitch);
        }

        const std::filesystem::path outputFilePath = EngineConfig::s_TextureDir / outputFileName;
        BenzinAssert(outputFilePath.extension() == ".dds");

        if (FAILED(DirectX::SaveToDDSFile(
            textureArray.GetImages(),
            textureArray.GetImageCount(),
            textureArray.GetMetadata(),
            DirectX::DDS_FLAGS_NONE,
            outputFilePath.c_str()
        )))
        {
            return false;
        }

        BenzinTrace("Texture array saved to: {}", outputFilePath.string());

        return true;
    }

}
