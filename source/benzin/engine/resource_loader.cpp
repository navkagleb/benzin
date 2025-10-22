#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/resource_loader.hpp>

#include <benzin/engine/gltf_reader.hpp>
#include <benzin/engine/mesh.hpp>

#include <DirectXTex.h>
#include <stb_image.h>

namespace benzin
{

    bool LoadTextureImageFromHdrFile(std::string_view fileName, TextureImage& textureImage)
    {
        const std::filesystem::path filePath = GetTextureDir() / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".hdr");

        const std::string narrowFilePath = filePath.string();

        BenzinAssert(stbi_is_hdr(narrowFilePath.c_str()) != 0);

        int width;
        int height;
        const int componentCount = 4;
        float* imageData = stbi_loadf(narrowFilePath.c_str(), &width, &height, nullptr, componentCount);

        if (imageData == nullptr)
            return false;

        textureImage.m_DebugName = fileName;
        textureImage.m_Format = GraphicsFormat::Rgba32Float;
        textureImage.m_Width = (uint32_t)width;
        textureImage.m_Height = (uint32_t)height;

        const uint32_t pixelDataSizeInBytes = width * height * GetFormatSizeInBytes(textureImage.m_Format);
        textureImage.m_PixelData.resize(pixelDataSizeInBytes);
        memcpy(textureImage.m_PixelData.data(), imageData, pixelDataSizeInBytes);

        stbi_image_free(imageData);

        return true;
    }

    bool LoadTextureImageFromDdsFile(std::string_view fileName, TextureImage& textureImage)
    {
        const std::filesystem::path filePath = GetTextureDir() / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".dds");

        DirectX::ScratchImage image;
        if (FAILED(DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)))
            return false;

        const DirectX::TexMetadata& metadata = image.GetMetadata();
        BenzinAssert(metadata.mipLevels == 1); // TODO: Add mip levels support
        BenzinAssert(magic_enum::enum_contains<GraphicsFormat>(metadata.format));

        textureImage.m_DebugName = fileName;
        textureImage.m_Format = (GraphicsFormat)metadata.format;
        textureImage.m_Width = (uint32_t)metadata.width;
        textureImage.m_Height = (uint32_t)metadata.height;
        textureImage.m_Depth = (uint16_t)metadata.arraySize;

        const size_t pixelDataSizeInBytes = image.GetPixelsSize();
        textureImage.m_PixelData.resize(pixelDataSizeInBytes);
        memcpy(textureImage.m_PixelData.data(), image.GetPixels(), pixelDataSizeInBytes);

        return true;
    }

    bool LoadMeshFromGltfFile(
        std::string_view fileName,
        Mesh& mesh,
        std::vector<MeshDrawPart>& meshDrawParts,
        std::vector<Material>& materials,
        std::vector<TextureImage>& textures)
    {
        static thread_local GltfReader s_GltfReader;

        return s_GltfReader.ReadFromFile(fileName, mesh, meshDrawParts, materials, textures);
    }

}
