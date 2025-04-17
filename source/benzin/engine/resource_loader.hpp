#pragma once

namespace benzin
{

    struct MeshResource;
    struct TextureImage;

    bool LoadTextureImageFromHdrFile(std::string_view fileName, TextureImage& outTextureImage);
    bool LoadTextureImageFromDdsFile(std::string_view fileName, TextureImage& outTextureImage);

    bool LoadMeshFromGltfFile(std::string_view fileName, MeshResource& outMesh);

}
