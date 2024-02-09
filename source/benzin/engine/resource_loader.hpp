#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    struct MeshData
    {
        std::vector<joint::MeshVertex> Vertices;
        std::vector<uint32_t> Indices;

        PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;

        std::optional<DirectX::BoundingBox> BoundingBox;
    };

    struct MeshNode
    {
        DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();
    };

    struct MeshInstance
    {
        uint32_t MeshIndex = g_InvalidIndex<uint32_t>;
        uint32_t MaterialIndex = g_InvalidIndex<uint32_t>;

        uint32_t MeshNodeIndex = g_InvalidIndex<uint32_t>; // Optional
    };

    struct TextureImage
    {
        std::string DebugName;

        GraphicsFormat Format = GraphicsFormat::Unknown;
        bool IsCubeMap = false;
        uint32_t Width = 0;
        uint32_t Height = 0;

        std::vector<std::byte> ImageData;
    };

    struct Material
    {
        uint32_t AlbedoTextureIndex = g_InvalidIndex<uint32_t>;
        uint32_t NormalTextureIndex = g_InvalidIndex<uint32_t>;
        uint32_t MetalRoughnessTextureIndex = g_InvalidIndex<uint32_t>;
        uint32_t EmissiveTextureIndex = g_InvalidIndex<uint32_t>;

        DirectX::XMFLOAT4 AlbedoFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float AlphaCutoff = 0.0f;
        float NormalScale = 1.0f;
        float MetalnessFactor = 1.0f;
        float RoughnessFactor = 1.0f;
        float OcclusionStrenght = 1.0f;
        DirectX::XMFLOAT3 EmissiveFactor{ 0.0f, 0.0f, 0.0f };
    };

    struct MeshCollectionResource
    {
        std::string DebugName;

        std::vector<MeshData> Meshes;
        std::vector<MeshNode> MeshNodes;
        std::vector<MeshInstance> MeshInstances;

        std::vector<TextureImage> TextureImages;
        std::vector<Material> Materials;
    };

    bool LoadMeshCollectionFromGLTFFile(std::string_view fileName, MeshCollectionResource& outMeshCollection);

} // namespace benzin
