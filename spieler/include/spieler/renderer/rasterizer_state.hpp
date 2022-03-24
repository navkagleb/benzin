#pragma once

#include "bindable.hpp"

namespace Spieler
{

    enum FillMode : std::uint32_t
    {
        FillMode_Wireframe = D3D12_FILL_MODE_WIREFRAME,
        FillMode_Solid = D3D12_FILL_MODE_SOLID,
    };

    enum CullMode : std::uint32_t
    {
        CullMode_None = D3D12_CULL_MODE_NONE,
        CullMode_Front = D3D12_CULL_MODE_FRONT,
        CullMode_Back = D3D12_CULL_MODE_BACK
    };

    struct RasterizerState
    {
        FillMode FillMode{ FillMode_Solid };
        CullMode CullMode{ CullMode_Back };
    };

} // namespace Spieler