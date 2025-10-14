#pragma once

#include <benzin/graphics/common.hpp>
#include <benzin/graphics/format.hpp>

namespace joint
{
    struct MeshInstance;
    struct Meshlet;
    struct MeshletCullVolume;
    struct MeshVertex;
}

namespace benzin
{

    class Buffer;
    class Device;

    struct MeshData
    {
        std::vector<joint::MeshVertex> Vertices;
        std::vector<uint32_t> Indices;

        PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;

        std::optional<DirectX::BoundingBox> BoundingBox;
    };

    struct MeshDrawRange
    {
        SubRange32 m_VertexRange;
        SubRange32 m_IndexRange;

        SubRange32 m_MeshletRange;
        SubRange32 m_MeshletIndirectVertexRange;
        SubRange32 m_MeshletIndexRange;

        PrimitiveTopology m_Topology = PrimitiveTopology::Unknown;

        DirectX::BoundingSphere m_BoundingSphere = {};
    };

    struct MeshInstance
    {
        DirectX::XMMATRIX m_ObjectToLocalMatrix = DirectX::XMMatrixIdentity();
        uint32_t m_DrawRangeIndex = g_Bad32;
        uint32_t m_MaterialIndex = g_Bad32; // Optional
    };

    struct MeshGpuStorage
    {
        std::unique_ptr<Buffer> VertexBuffer;
        std::unique_ptr<Buffer> IndexBuffer;

        std::unique_ptr<Buffer> MeshletBuffer;
        std::unique_ptr<Buffer> MeshletCullVolumeBuffer;
        std::unique_ptr<Buffer> MeshletIndirectVertexBuffer;
        std::unique_ptr<Buffer> MeshletIndexBuffer;
    };

    struct Mesh
    {
        std::vector<joint::MeshVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        std::vector<MeshDrawRange> m_DrawRanges;
        std::vector<MeshInstance> m_Instances;

        std::vector<joint::Meshlet> m_Meshlets;
        std::vector<joint::MeshletCullVolume> m_MeshletCullVolumes;
        std::vector<uint32_t> m_MeshletIndirectVertices; // Can be used uint16_t if Vertices.size() <= std::numeric_limits<uint16_t>::max()
        std::vector<uint8_t> m_MeshletIndices;

        std::span<const joint::MeshVertex> GetDrawRangeVertices(const MeshDrawRange& drawRange) const;
        std::span<const uint32_t> GetDrawRangeIndices(const MeshDrawRange& drawRange) const;

        MeshGpuStorage CreateGpuStorage(Device& device, std::string_view debugName) const;
    };

    struct MaterialTextureIndices
    {
        uint32_t m_Albedo = g_Bad32;
        uint32_t m_Normal = g_Bad32;
        uint32_t m_MetallicRoughness = g_Bad32;
        uint32_t m_Emissive = g_Bad32;
    };

    struct MaterialConsts
    {
        DirectX::XMFLOAT4 m_AlbedoFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float m_AlphaCutoff = 0.0f;
        float m_NormalScale = 1.0f;
        float m_MetalnessFactor = 1.0f;
        float m_RoughnessFactor = 1.0f;
        float m_OcclusionStrenght = 1.0f;
        DirectX::XMFLOAT3 m_EmissiveFactor{ 0.0f, 0.0f, 0.0f };

        bool m_IsAlphaTestRequired = false;
    };

    // Types for loading from disk

    struct MeshResource
    {
        std::vector<joint::MeshVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        std::vector<MeshDrawRange> m_DrawRanges;
        std::vector<MeshInstance> m_Instances;
    };

    struct MaterialResource
    {
        MaterialTextureIndices m_TextureIndices;
        MaterialConsts m_Consts;
    };

    struct TextureImage
    {
        std::string m_DebugName;

        GraphicsFormat m_Format = GraphicsFormat::Unknown;
        bool m_IsCubeMap = false;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint16_t m_Depth = 1;

        std::vector<std::byte> m_PixelData;
    };

}
