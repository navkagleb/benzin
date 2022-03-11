#pragma once

#include <unordered_map>

#include "resources/vertex_buffer.h"
#include "resources/index_buffer.h"

namespace Spieler
{

    struct SubmeshGeometry
    {
        std::uint32_t IndexCount            = 0;
        std::uint32_t StartIndexLocation    = 0;
        std::uint32_t BaseVertexLocation    = 0;
    };

    struct MeshGeometry
    {
        using SubmeshContainer = std::unordered_map<std::string, SubmeshGeometry>;

        VertexBuffer        VertexBuffer;
        IndexBuffer         IndexBuffer;
        SubmeshContainer    Submeshes;
    };

} // namespace Spieler