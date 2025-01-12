#pragma once

namespace joint
{

    struct Material;
    struct MeshInstance;

}

namespace benzin
{

    struct MeshData;

    struct TextureImage
    {
        std::string DebugName;

        GraphicsFormat Format = GraphicsFormat::Unknown;
        bool IsCubeMap = false;
        uint32_t Width = 0;
        uint32_t Height = 0;

        std::vector<std::byte> ImageData;
    };

    struct MeshResource
    {
        std::string DebugName;

        std::vector<MeshData> SubMeshes;
        std::vector<joint::MeshInstance> SubMeshInstances;

        std::vector<TextureImage> TextureImages;
        std::vector<joint::Material> Materials;
    };

    bool LoadTextureImageFromHdrFile(std::string_view fileName, TextureImage& textureImage);
    bool LoadTextureImageFromDdsFile(std::string_view fileName, TextureImage& textureImage);

    bool LoadMeshFromGltfFile(std::string_view fileName, MeshResource& outMesh);

}
