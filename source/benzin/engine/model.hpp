#pragma once

#include "benzin/graphics/texture.hpp"

namespace benzin
{
    
    class Buffer;

    template <std::integral T>
    using IterableRange = std::ranges::iota_view<T, T>;

    struct TextureResourceData
    {
        TextureCreation TextureCreation;
        std::vector<std::byte> ImageData;
    };

    struct MeshVertexData
    {
        DirectX::XMFLOAT3 LocalPosition{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 LocalNormal{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT2 TexCoord{ 0.0f, 0.0f };
    };

    struct MeshPrimitiveData
    {
        std::vector<MeshVertexData> Vertices;
        std::vector<uint32_t> Indices;

        PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;
    };

    struct MaterialData
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

    class Mesh
    {
    public:
        struct Primitive
        {
            uint32_t IndexCount = 0;
            PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;
        };

    public:
        explicit Mesh(Device& device);
        Mesh(Device& device, const std::vector<MeshPrimitiveData>& meshPrimitivesData, std::string_view name);

    public:
        const std::shared_ptr<Buffer>& GetVertexBuffer() const { return m_VertexBuffer; }
        const std::shared_ptr<Buffer>& GetIndexBuffer() const { return m_IndexBuffer; }
        const std::shared_ptr<Buffer>& GetPrimitiveBuffer() const { return m_PrimitiveBuffer; }

        const std::vector<Primitive>& GetPrimitives() const { return m_Primitives; }

    public:
        void CreateFromPrimitives(const std::vector<MeshPrimitiveData>& meshPrimitivesData, std::string_view name);

    private:
        void CreateBuffers(uint32_t vertexCount, uint32_t indexCount, uint32_t primitiveCount, std::string_view name);
        void FillBuffers(const std::vector<MeshPrimitiveData>& meshPrimitivesData);

    private:
        Device& m_Device;

        std::shared_ptr<Buffer> m_VertexBuffer;
        std::shared_ptr<Buffer> m_IndexBuffer;
        std::shared_ptr<Buffer> m_PrimitiveBuffer;

        std::vector<Primitive> m_Primitives;
    };

    struct NodeData
    {
        IterableRange<uint32_t> DrawPrimitiveRange;
        DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();
    };

    class Model
    {
    public:
        struct Node
        {
            IterableRange<uint32_t> DrawPrimitiveRange;
        };

        struct DrawPrimitive
        {
            uint32_t MeshPrimitiveIndex = 0;
            uint32_t MaterialIndex = 0;
        };

    public:
        explicit Model(Device& device);
        Model(Device& device, std::string_view fileName);
        Model(
            Device& device,
            const std::shared_ptr<Mesh>& mesh,
            const std::vector<DrawPrimitive>& drawPrimitives,
            const std::vector<MaterialData>& materialsData,
            std::string_view name
        );

    public:
        const std::shared_ptr<Mesh>& GetMesh() const { return m_Mesh; }

        const std::unique_ptr<Buffer>& GetDrawPrimitiveBuffer() const { return m_DrawPrimitiveBuffer; }
        const std::vector<DrawPrimitive>& GetDrawPrimitives() const { return m_DrawPrimitives; }

        const std::shared_ptr<Buffer>& GetNodeBuffer() const { return m_NodeBuffer; }
        const std::vector<Node>& GetNodes() const { return m_Nodes; }

        const std::shared_ptr<Buffer>& GetMaterialBuffer() const { return m_MaterialBuffer; }
        std::vector<MaterialData>& GetMaterials() { return m_Materials; }
        const std::vector<MaterialData>& GetMaterials() const { return m_Materials; }

    public:
        bool LoadFromGLTFFile(std::string_view fileName);
        
        void CreateFrom(
            const std::shared_ptr<Mesh>& mesh,
            const std::vector<DrawPrimitive>& drawPrimitives,
            const std::vector<MaterialData>& materialsData,
            std::string_view name
        );

    private:
        void CreateDrawPrimitivesBuffer(const std::vector<DrawPrimitive>& drawPrimitives);
        void CreateNodes(const std::vector<NodeData>& nodesData);
        void CreateTextures(const std::vector<TextureResourceData>& textureResourcesData);
        void CreateMaterialBuffer(const std::vector<MaterialData>& materialsData);

    private:
        Device& m_Device;

        std::string m_Name;

        std::shared_ptr<Mesh> m_Mesh;

        std::unique_ptr<Buffer> m_DrawPrimitiveBuffer;
        std::vector<DrawPrimitive> m_DrawPrimitives;

        std::shared_ptr<Buffer> m_NodeBuffer;
        std::vector<Node> m_Nodes;

        std::vector<std::shared_ptr<Texture>> m_Textures;
        std::shared_ptr<Buffer> m_MaterialBuffer;
        std::vector<MaterialData> m_Materials;
    };

} // namespace benzin
