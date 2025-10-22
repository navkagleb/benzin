#pragma once

namespace benzin
{

    struct Material;
    struct Mesh;
    struct MeshDrawPart;
    struct TextureImage;

    bool LoadTextureImageFromHdrFile(std::string_view fileName, TextureImage& textureImage);
    bool LoadTextureImageFromDdsFile(std::string_view fileName, TextureImage& textureImage);

    bool LoadMeshFromGltfFile(
        std::string_view fileName,
        Mesh& mesh,
        std::vector<MeshDrawPart>& meshDrawParts,
        std::vector<Material>& materials,
        std::vector<TextureImage>& textures);

}
