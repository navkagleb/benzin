#pragma once

#include <unordered_map>

#include "primitive_topology.h"
#include "resources/vertex_buffer.h"
#include "resources/index_buffer.h"

namespace Spieler
{

    struct SubmeshGeometry
    {
        std::uint32_t IndexCount{ 0 };
        std::uint32_t StartIndexLocation{ 0 };
        std::uint32_t BaseVertexLocation{ 0 };
    };

    struct MeshGeometry
    {
        using SubmeshContainer = std::unordered_map<std::string, SubmeshGeometry>;

        VertexBuffer        VertexBuffer;
        IndexBuffer         IndexBuffer;
        SubmeshContainer    Submeshes; 
        PrimitiveTopology   PrimitiveTopology{ PrimitiveTopology_Undefined };
    };

    struct RenderItem
    {
        DirectX::XMMATRIX   World{ DirectX::XMMatrixIdentity() };
        std::int32_t        ConstantBufferIndex{ -1 };
        std::int32_t        DirtyFrameCount{ 3 };
        MeshGeometry*       MeshGeometry{ nullptr };
        SubmeshGeometry*    SubmeshGeometry{ nullptr };
    };

} // namespace Spieler