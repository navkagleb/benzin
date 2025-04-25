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
        uint32_t VertexOffset = 0;
        uint32_t IndexOffset = 0;

        uint32_t VertexCount = g_Bad32;
        uint32_t IndexCount = g_Bad32;

        PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;
    };

    struct MeshInstance
    {
        DirectX::XMMATRIX LocalTransform = DirectX::XMMatrixIdentity();
        uint32_t DrawRangeIndex = g_Bad32;
        uint32_t MaterialIndex = g_Bad32; // Optional
    };

    struct MeshGpuStorage
    {
        std::unique_ptr<Buffer> VertexBuffer;
        std::unique_ptr<Buffer> IndexBuffer;
        std::unique_ptr<Buffer> InstanceTransformBuffer;
    };

    struct Mesh
    {
        std::vector<joint::MeshVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<MeshDrawRange> DrawRanges;
        std::vector<MeshInstance> Instances;

        bool IsIndexOrderClockwise = true;

        MeshGpuStorage CreateGpuStorage(Device& device, std::string_view debugName) const;
    };

    struct MaterialTextureIndices
    {
        uint32_t Albedo = g_Bad32;
        uint32_t Normal = g_Bad32;
        uint32_t MetallicRoughness = g_Bad32;
        uint32_t Emissive = g_Bad32;
    };

    struct MaterialConsts
    {
        DirectX::XMFLOAT4 AlbedoFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float AlphaCutoff = 0.0f;
        float NormalScale = 1.0f;
        float MetalnessFactor = 1.0f;
        float RoughnessFactor = 1.0f;
        float OcclusionStrenght = 1.0f;
        DirectX::XMFLOAT3 EmissiveFactor{ 0.0f, 0.0f, 0.0f };

        bool IsAlphaTestRequired = false;
    };

    // Types for loading from disk

    struct TextureImage
    {
        std::string DebugName;

        GraphicsFormat Format = GraphicsFormat::Unknown;
        bool IsCubeMap = false;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint16_t Depth = 1;

        std::vector<std::byte> PixelData;
    };

    struct MeshResource
    {
        struct Material
        {
            MaterialTextureIndices TextureIndices;
            MaterialConsts Consts;
        };

        std::string DebugName;

        std::vector<joint::MeshVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<MeshDrawRange> DrawRanges;

        std::vector<Material> Materials;
        std::vector<TextureImage> TextureImages;

        std::vector<MeshInstance> Instances;

        bool IsIndexOrderClockwise = true;
    };

}
