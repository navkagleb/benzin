#pragma once

namespace benzin
{

    struct Material;
    struct MeshDraw;
    struct MeshGeometry;
    struct TextureImage;

    bool LoadTextureImageFromHdrFile(std::string_view fileName, TextureImage& textureImage);
    bool LoadTextureImageFromDdsFile(std::string_view fileName, TextureImage& textureImage);

    bool LoadMeshFromGltfFile(
        std::string_view fileName,
        MeshGeometry& geometry,
        std::vector<MeshDraw>& meshDraws,
        std::vector<Material>& materials,
        std::vector<TextureImage>& textures);

}
