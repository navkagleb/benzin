#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/resource_loader.hpp>

#include <DirectXTex.h>
#include <stb_image.h>

#include <benzin/engine/gltf_reader.hpp>
#include <benzin/engine/mesh.hpp>

namespace benzin
{

    bool LoadTextureImageFromHdrFile(std::string_view fileName, TextureImage& textureImage)
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

        textureImage.DebugName = fileName;
        textureImage.Format = GraphicsFormat::Rgba32Float;
        textureImage.Width = (uint32_t)width;
        textureImage.Height = (uint32_t)height;

        const Bytes imageSize = width * height * GetFormatSize(textureImage.Format);
        textureImage.ImageData.resize(imageSize);
        memcpy(textureImage.ImageData.data(), imageData, imageSize);

        stbi_image_free(imageData);

        return true;
    }

    bool LoadTextureImageFromDdsFile(std::string_view fileName, TextureImage& textureImage)
    {
        BenzinUnused(textureImage);

        const std::filesystem::path filePath = EngineConfig::s_TextureDir / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".dds");

        DirectX::ScratchImage image;
        if (FAILED(DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)))
        {
            return false;
        }

        const auto& metadata = image.GetMetadata();
        BenzinAssert(image.GetImageCount() == 1);
        BenzinAssert(metadata.mipLevels == 1); // TODO: Add mip levels support
        BenzinAssert(magic_enum::enum_contains<GraphicsFormat>(metadata.format));

        textureImage.DebugName = fileName;
        textureImage.Format = (GraphicsFormat)metadata.format;
        textureImage.Width = (uint32_t)metadata.width;
        textureImage.Height = (uint32_t)metadata.height;

        const Bytes dataSize = image.GetPixelsSize();
        textureImage.ImageData.resize(dataSize);
        memcpy(textureImage.ImageData.data(), image.GetPixels(), dataSize);

        return true;
    }

    bool LoadMeshFromGltfFile(std::string_view fileName, MeshResource& outMesh)
    {
        static thread_local GltfReader gltfReader;

        return gltfReader.ReadFromFile(fileName, outMesh);
    }

}
