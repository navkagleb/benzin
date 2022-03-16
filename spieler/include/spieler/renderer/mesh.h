#pragma once

#include <unordered_map>

#include "primitive_topology.h"
#include "vertex_buffer.h"
#include "index_buffer.h"
#include "constant_buffer.h"

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
        ConstantBuffer      ConstantBuffer;
        MeshGeometry*       MeshGeometry{ nullptr };
        SubmeshGeometry*    SubmeshGeometry{ nullptr };
    };

} // namespace Spieler