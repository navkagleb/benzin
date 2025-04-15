#pragma once

#include "benzin/graphics/common.hpp"

namespace joint
{
    struct MeshInstance;
    struct MeshVertex;
}

namespace benzin
{

    class Buffer;

    struct MeshData
    {
        std::vector<joint::MeshVertex> Vertices;
        std::vector<uint32_t> Indices;

        PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;

        std::optional<DirectX::BoundingBox> BoundingBox;
    };

    struct MeshInfo
    {
        uint32_t VertexOffset = 0;
        uint32_t IndexOffset = 0;
    };

    struct Material
    {
        uint32_t AlbedoTextureIndex = g_Bad32;
        uint32_t NormalTextureIndex = g_Bad32;
        uint32_t MetallicRoughnessTextureIndex = g_Bad32;
        uint32_t EmissiveTextureIndex = g_Bad32;

        DirectX::XMFLOAT4 AlbedoFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float AlphaCutoff = 0.0f;
        float NormalScale = 1.0f;
        float MetalnessFactor = 1.0f;
        float RoughnessFactor = 1.0f;
        float OcclusionStrenght = 1.0f;
        DirectX::XMFLOAT3 EmissiveFactor{ 0.0f, 0.0f, 0.0f };

        bool IsAlphaTestRequired = false;
    };

    struct Mesh
    {
        std::vector<MeshData> SubMeshes;
        std::vector<MeshInfo> SubMeshInfos;
        std::vector<Material> Materials;
        std::vector<joint::MeshInstance> SubMeshInstances;

        uint32_t TotalVertexCount = 0;
        uint32_t TotalIndexCount = 0;

        bool IsIndexOrderClockwise = true;
    };

    struct MeshGpuStorage
    {
        std::unique_ptr<Buffer> VertexBuffer;
        std::unique_ptr<Buffer> IndexBuffer;
        std::unique_ptr<Buffer> MeshInstanceBuffer;
        std::unique_ptr<Buffer> MaterialBuffer;
    };

    // Types for loading from disk

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
        std::vector<Material> Materials;

        bool IsIndexOrderClockwise = true;
    };

}
