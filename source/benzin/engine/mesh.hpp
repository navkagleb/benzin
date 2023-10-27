#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Buffer;
    class Device;

    struct MeshVertex
    {
        DirectX::XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Normal{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT2 TexCoord{ 0.0f, 0.0f };
    };

    struct MeshData
    {
        std::vector<MeshVertex> Vertices;
        std::vector<uint32_t> Indices;

        PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;
    };

    struct MeshCreation
    {
        std::string_view DebugName;
        std::vector<MeshData> Meshes;
        bool IsNeedSplitByMeshes = true;
    };

    class Mesh
    {
    public:
        struct SubMesh
        {
            uint32_t IndexCount = 0;
            PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;
        };

    public:
        explicit Mesh(Device& device);
        Mesh(Device& device, const MeshCreation& creation);

    public:
        const std::shared_ptr<Buffer>& GetVertexBuffer() const { return m_VertexBuffer; }
        const std::shared_ptr<Buffer>& GetIndexBuffer() const { return m_IndexBuffer; }
        const std::shared_ptr<Buffer>& GetSubMeshBuffer() const { return m_SubMeshBuffer; }

        const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }

    public:
        void Create(const MeshCreation& creation);

    private:
        void CreateBuffers(uint32_t vertexCount, uint32_t indexCount, uint32_t primitiveCount);
        void FillBuffers(const std::vector<MeshData>& meshPrimitivesData);

    private:
        Device& m_Device;

        std::string_view m_DebugName;

        std::shared_ptr<Buffer> m_VertexBuffer;
        std::shared_ptr<Buffer> m_IndexBuffer;
        std::shared_ptr<Buffer> m_SubMeshBuffer;

        std::vector<SubMesh> m_SubMeshes;
    };

} // namespace
