#pragma once

#include "bindable.hpp"

namespace Spieler
{

    enum class FillMode : std::uint32_t
    {
        Wireframe = D3D12_FILL_MODE_WIREFRAME,
        Solid = D3D12_FILL_MODE_SOLID,
    };

    enum class CullMode : std::uint32_t
    {
        None = D3D12_CULL_MODE_NONE,
        Front = D3D12_CULL_MODE_FRONT,
        Back = D3D12_CULL_MODE_BACK
    };

    struct RasterizerState
    {
        FillMode FillMode{ FillMode::Solid };
        CullMode CullMode{ CullMode::Back };
    };

} // namespace Spieler