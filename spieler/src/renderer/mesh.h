#pragma once

#include <cstdint>

namespace Spieler
{

    struct SubmeshGeometry
    {
        std::uint32_t IndexCount            = 0;
        std::uint32_t StartIndexLocation    = 0;
        std::uint32_t BaseVertexLocation    = 0;
    };

} // namespace Spieler