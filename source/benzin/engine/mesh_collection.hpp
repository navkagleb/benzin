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

    using MeshIndex = uint32_t;

    struct MeshCollectionCreation
    {
        std::string_view DebugName;
        std::vector<MeshData> Meshes;
        bool IsNeedSplitByMeshes = true;
    };

    class MeshCollection
    {
    public:
        struct MeshInfo
        {
            uint32_t StartVertex = 0;
            uint32_t StartIndex = 0;
            uint32_t IndexCount = 0;
            PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;
        };

    public:
        explicit MeshCollection(Device& device);
        MeshCollection(Device& device, const MeshCollectionCreation& creation);

    public:
        const std::shared_ptr<Buffer>& GetVertexBuffer() const { return m_VertexBuffer; }
        const std::shared_ptr<Buffer>& GetIndexBuffer() const { return m_IndexBuffer; }
        const std::shared_ptr<Buffer>& GetMeshInfoBuffer() const { return m_MeshInfoBuffer; }

        std::span<const MeshInfo> GetMeshInfos() const { return m_MeshInfos; }

    public:
        void Create(const MeshCollectionCreation& creation);

    private:
        Device& m_Device;

        std::string_view m_DebugName;

        std::shared_ptr<Buffer> m_VertexBuffer;
        std::shared_ptr<Buffer> m_IndexBuffer;
        std::shared_ptr<Buffer> m_MeshInfoBuffer;

        std::vector<MeshInfo> m_MeshInfos;
    };

} // namespace benzin
