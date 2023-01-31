#pragma once

namespace benzin
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
        int32_t DepthBias{ D3D12_DEFAULT_DEPTH_BIAS };
        float DepthBiasClamp{ D3D12_DEFAULT_DEPTH_BIAS_CLAMP };
        float SlopeScaledDepthBias{ D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS };
    };

} // namespace benzin
