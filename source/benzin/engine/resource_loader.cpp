#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/resource_loader.hpp>

#include <DirectXTex.h>
#include <stb_image.h>

#include <benzin/engine/gltf_reader.hpp>
#include <benzin/engine/mesh.hpp>

namespace benzin
{

    bool LoadTextureImageFromHdrFile(std::string_view fileName, TextureImage& outTextureImage)
    {
        const std::filesystem::path filePath = EngineConfig::s_TextureDir / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".hdr");

        const std::string narrowFilePath = filePath.string();

        BenzinAssert(stbi_is_hdr(narrowFilePath.c_str()) != 0);

        int width;
        int height;
        const int componentCount = 4;
        float* imageData = stbi_loadf(narrowFilePath.c_str(), &width, &height, nullptr, componentCount);

        if (!imageData)
        {
            return false;
        }

        outTextureImage.DebugName = fileName;
        outTextureImage.Format = GraphicsFormat::Rgba32Float;
        outTextureImage.Width = (uint32_t)width;
        outTextureImage.Height = (uint32_t)height;

        const uint32_t pixelDataSizeInBytes = width * height * GetFormatSizeInBytes(outTextureImage.Format);
        outTextureImage.PixelData.resize(pixelDataSizeInBytes);
        memcpy(outTextureImage.PixelData.data(), imageData, pixelDataSizeInBytes);

        stbi_image_free(imageData);

        return true;
    }

    bool LoadTextureImageFromDdsFile(std::string_view fileName, TextureImage& outTextureImage)
    {
        const std::filesystem::path filePath = EngineConfig::s_TextureDir / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".dds");

        DirectX::ScratchImage image;
        if (FAILED(DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)))
        {
            return false;
        }

        const DirectX::TexMetadata& metadata = image.GetMetadata();
        BenzinAssert(metadata.mipLevels == 1); // TODO: Add mip levels support
        BenzinAssert(magic_enum::enum_contains<GraphicsFormat>(metadata.format));

        outTextureImage.DebugName = fileName;
        outTextureImage.Format = (GraphicsFormat)metadata.format;
        outTextureImage.Width = (uint32_t)metadata.width;
        outTextureImage.Height = (uint32_t)metadata.height;
        outTextureImage.Depth = (uint16_t)metadata.arraySize;

        const size_t pixelDataSizeInBytes = image.GetPixelsSize();
        outTextureImage.PixelData.resize(pixelDataSizeInBytes);
        memcpy(outTextureImage.PixelData.data(), image.GetPixels(), pixelDataSizeInBytes);

        return true;
    }

    bool LoadMeshFromGltfFile(std::string_view fileName, MeshResource& outMesh)
    {
        static thread_local GltfReader gltfReader;

        return gltfReader.ReadFromFile(fileName, outMesh);
    }

}
