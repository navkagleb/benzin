#pragma once

namespace tinygltf
{
    struct Mesh;
    struct Primitive;

    class Model;
    class TinyGLTF;
}

namespace benzin
{

    struct Material;
    struct Mesh;
    struct MeshDrawPart;
    struct TextureImage;

    class GltfReader
    {
    public:
        GltfReader();
        ~GltfReader();

        bool ReadFromFile(
            std::string_view fileName,
            Mesh& mesh,
            std::vector<MeshDrawPart>& meshDrawParts,
            std::vector<Material>& materials,
            std::vector<TextureImage>& textures);

    private:
        template <typename T>
        std::span<const T> ParseGltfAccessor(int gltfAccessorIndex);

        template <std::integral IndexType>
        void ParseGltfPrimitive(const tinygltf::Primitive& gltfPrimitive, Mesh& mesh);
        void ParseGltfMeshes(Mesh& mesh);
        void ParseGltfNode(int gltfNodeIndex, const DirectX::XMMATRIX& parentObjectToLocal, std::vector<MeshDrawPart>& meshDrawParts);
        void ParseGltfNodes(std::vector<MeshDrawPart>& meshDrawParts);
        void ParseGltfMaterials(std::vector<Material>& materials);
        void ParseGltfTextures(std::vector<TextureImage>& textures);

        uint32_t AddTextureMapping(int gltfTextureIndex, bool isSrgb);

        struct TextureMapping
        {
            uint16_t m_MappedIndex : 15 = 0;
            uint16_t m_IsSrgb : 1 = false;
        };

        std::unique_ptr<tinygltf::TinyGLTF> m_GltfContext;
        std::unique_ptr<tinygltf::Model> m_GltfModel;
        std::unordered_map<uint32_t, TextureMapping> m_TextureMappings;
    };

}
