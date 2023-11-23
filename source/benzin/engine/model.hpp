#pragma once

#include "benzin/graphics/texture.hpp"

namespace benzin
{
    
    class Buffer;
    class MeshCollection;

    struct TextureImage
    {
        TextureCreation TextureCreation;
        std::vector<std::byte> ImageData;
    };

    struct Material
    {
        static constexpr const uint32_t s_InvalidTextureIndex = std::numeric_limits<uint32_t>::max();

        uint32_t AlbedoTextureIndex = s_InvalidTextureIndex;
        uint32_t NormalTextureIndex = s_InvalidTextureIndex;
        uint32_t MetalRoughnessTextureIndex = s_InvalidTextureIndex;
        uint32_t AOTextureIndex = s_InvalidTextureIndex;
        uint32_t EmissiveTextureIndex = s_InvalidTextureIndex;

        DirectX::XMFLOAT4 AlbedoFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float AlphaCutoff = 0.0f;
        float NormalScale = 1.0f;
        float MetalnessFactor = 1.0f;
        float RoughnessFactor = 1.0f;
        float OcclusionStrenght = 1.0f;
        DirectX::XMFLOAT3 EmissiveFactor{ 0.0f, 0.0f, 0.0f };
    };

    struct DrawableMesh
    {
        uint32_t MeshIndex = 0;
        uint32_t MaterialIndex = 0;
    };

    struct ModelCreation
    {
        std::string_view DebugName;

        std::shared_ptr<MeshCollection> MeshCollection;
        std::vector<DrawableMesh> DrawableMeshes;
        std::vector<Material> Materials;
    };

    class Model
    {
    public:
        struct Node
        {
            IterableRange<uint32_t> DrawableMeshIndexRange;
            DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();
        };

    public:
        explicit Model(Device& device);
        Model(Device& device, std::string_view fileName);
        Model(Device& device, const ModelCreation& creation);

    public:
        const std::shared_ptr<MeshCollection>& GetMeshCollection() const { return m_MeshCollection; }

        const std::vector<DrawableMesh>& GetDrawableMeshes() const { return m_DrawableMeshes; }
        const std::vector<Node>& GetNodes() const { return m_Nodes; }
        const std::vector<Material>& GetMaterials() const { return m_Materials; }

        const std::shared_ptr<Buffer>& GetDrawableMeshBuffer() const { return m_DrawableMeshBuffer; }
        const std::shared_ptr<Buffer>& GetNodeBuffer() const { return m_NodeBuffer; }
        const std::shared_ptr<Buffer>& GetMaterialBuffer() const { return m_MaterialBuffer; }

    public:
        void LoadFromFile(std::string_view fileName);
        void Create(const ModelCreation& creation);

    private:
        void CreateDrawableMeshBuffer();
        void CreateNodeBuffer();
        void CreateTextures(const std::vector<TextureImage>& textureImages);
        void CreateMaterialBuffer();

    private:
        Device& m_Device;

        std::string m_DebugName;

        std::shared_ptr<MeshCollection> m_MeshCollection;

        std::vector<DrawableMesh> m_DrawableMeshes;
        std::vector<Node> m_Nodes;
        std::vector<Material> m_Materials;

        std::shared_ptr<Buffer> m_DrawableMeshBuffer;
        std::shared_ptr<Buffer> m_NodeBuffer;
        std::vector<std::shared_ptr<Texture>> m_Textures;
        std::shared_ptr<Buffer> m_MaterialBuffer;
    };

} // namespace benzin
