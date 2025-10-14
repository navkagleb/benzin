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

    struct MaterialResource;
    struct MeshResource;
    struct TextureImage;

    class GltfReader
    {
    public:
        GltfReader();
        ~GltfReader();

        bool ReadFromFile(
            std::string_view fileName,
            MeshResource& mesh,
            std::vector<MaterialResource>& materials,
            std::vector<TextureImage>& textures);

    private:
        template <typename T>
        std::span<const T> ParseGltfAccessor(int gltfAccessorIndex);

        template <std::integral IndexType>
        void ParseGltfPrimitive(const tinygltf::Primitive& gltfPrimitive, MeshResource& mesh);
        void ParseGltfMeshes(MeshResource& meshResource);
        void ParseGltfNode(int gltfNodeIndex, const DirectX::XMMATRIX& parentObjectToLocal, MeshResource& mesh);
        void ParseGltfNodes(MeshResource& mesh);
        void ParseGltfMaterials(std::vector<MaterialResource>& materials);
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
