#pragma once

namespace spieler::renderer
{

    enum class FillMode : uint8_t
    {
        Wireframe = D3D12_FILL_MODE_WIREFRAME,
        Solid = D3D12_FILL_MODE_SOLID,
    };

    enum class CullMode : uint8_t
    {
        None = D3D12_CULL_MODE_NONE,
        Front = D3D12_CULL_MODE_FRONT,
        Back = D3D12_CULL_MODE_BACK
    };

    enum class TriangleOrder : bool
    {
        Clockwise = false,
        Counterclockwise = true
    };

    struct RasterizerState
    {
        FillMode FillMode{ FillMode::Solid };
        CullMode CullMode{ CullMode::Back };
        TriangleOrder TriangleOrder{ TriangleOrder::Clockwise };
    };

} // namespace spieler::renderer
