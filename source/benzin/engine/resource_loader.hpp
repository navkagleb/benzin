#pragma once

namespace benzin
{

    struct MaterialResource;
    struct MeshResource;
    struct TextureImage;

    bool LoadTextureImageFromHdrFile(std::string_view fileName, TextureImage& textureImage);
    bool LoadTextureImageFromDdsFile(std::string_view fileName, TextureImage& textureImage);

    bool LoadMeshFromGltfFile(
        std::string_view fileName,
        MeshResource& mesh,
        std::vector<MaterialResource>& materials,
        std::vector<TextureImage>& textures);

}
