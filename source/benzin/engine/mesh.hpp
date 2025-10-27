#pragma once

#include <benzin/graphics/format.hpp>

namespace joint
{
    struct Meshlet;
    struct MeshletCullVolume;
    struct MeshVertex;
}

namespace benzin
{

    struct MeshPart
    {
        uint32_t m_VertexOffset = 0;
        uint32_t m_VertexCount = 0;
        uint32_t m_IndexOffset = 0;
        uint32_t m_IndexCount = 0;

        uint32_t m_MeshletOffset = 0;
        uint32_t m_MeshletCount = 0;
        uint32_t m_MeshletVertexIndexOffset = 0;
        uint32_t m_MeshletVertexIndexCount = 0;
        uint32_t m_MeshletIndexOffset = 0;
        uint32_t m_MeshletIndexCount = 0;
    };

    struct MeshDrawPart
    {
        DirectX::XMMATRIX m_ObjectToLocal = DirectX::XMMatrixIdentity();
        uint32_t m_PartIndex = g_MaxU32;
        uint32_t m_MaterialIndex = g_MaxU32;
    };

    struct Mesh
    {
        std::vector<joint::MeshVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        std::vector<MeshPart> m_Parts;

        std::vector<joint::Meshlet> m_Meshlets;
        std::vector<joint::MeshletCullVolume> m_MeshletCullVolumes;
        std::vector<uint32_t> m_MeshletVertexIndices;
        std::vector<uint8_t> m_MeshletIndices;
    };

    struct Material
    {
        uint32_t m_AlbedoTextureIndex = g_MaxU32;
        uint32_t m_NormalTextureIndex = g_MaxU32;
        uint32_t m_MetallicRoughnessTextureIndex = g_MaxU32;
        uint32_t m_EmissiveTextureIndex = g_MaxU32;

        DirectX::XMFLOAT4 m_AlbedoFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float m_AlphaCutoff = 0.0f;
        float m_NormalScale = 1.0f;
        float m_MetalnessFactor = 1.0f;
        float m_RoughnessFactor = 1.0f;
        DirectX::XMFLOAT3 m_EmissiveFactor{ 0.0f, 0.0f, 0.0f };

        bool m_IsAlphaTestRequired = false;
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
