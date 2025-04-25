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

    struct MeshResource;

    class GltfReader
    {
    public:
        GltfReader();
        ~GltfReader();

        bool ReadFromFile(std::string_view fileName, MeshResource& outMesh);

    private:
        template <typename T>
        std::span<const T> ParseGltfAccessor(int gltfAccessorIndex);

        template <std::integral IndexType>
        void ParseGltfPrimitive(const tinygltf::Primitive& gltfPrimitive);

        void ParseGltfMesh(const tinygltf::Mesh& gltfMesh);
        void ParseGltfMeshes();
        void ParseGltfNode(int gltfNodeIndex, const DirectX::XMMATRIX& parentNodeTransform);
        void ParseGltfNodes();
        void ParseGltfMaterials();
        void ParseGltfTextures();

        uint32_t AddTextureMapping(int gltfTextureIndex, bool isSrgb);

        void ResetState();

    private:
        struct TextureMapping
        {
            uint16_t MappedIndex : 15 = 0;
            uint16_t IsSrgb : 1 = false;
        };

        std::unique_ptr<tinygltf::TinyGLTF> m_GltfContext;
        std::unique_ptr<tinygltf::Model> m_GltfModel;
        std::unordered_map<uint32_t, TextureMapping> m_TextureMappings;

        MeshResource* m_OutMesh = nullptr;
    };

}
