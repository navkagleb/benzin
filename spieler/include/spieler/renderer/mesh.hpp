#pragma once

#include <unordered_map>

#include "primitive_topology.hpp"
#include "vertex_buffer.hpp"
#include "index_buffer.hpp"
#include "constant_buffer.hpp"

namespace Spieler
{

    struct SubmeshGeometry
    {
        std::uint32_t IndexCount{ 0 };
        std::uint32_t BaseVertexLocation{ 0 };
        std::uint32_t StartIndexLocation{ 0 };
    };

    struct MeshGeometry
    {
        VertexBuffer VertexBuffer;
        IndexBuffer IndexBuffer;
        std::unordered_map<std::string, SubmeshGeometry> Submeshes;
        PrimitiveTopology PrimitiveTopology{ PrimitiveTopology_Undefined };
    };

    struct Material
    {
        std::int32_t DiffuseSRVHeapIndex{ -1 };
        std::uint32_t DirtyFrameCount{ 0 };
        ConstantBuffer ConstantBuffer;
    };

    struct MaterialConstants
    {
        DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 FrensnelR0{ 0.01f, 0.01f, 0.01f };
        float Roughness{ 0.25f };
        DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
    };

    struct RenderItem
    {
        ConstantBuffer ConstantBuffer;
        MeshGeometry* MeshGeometry{ nullptr };
        SubmeshGeometry* SubmeshGeometry{ nullptr };
        Material* Material{ nullptr };
    };

} // namespace Spieler