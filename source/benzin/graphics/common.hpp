#pragma once

namespace benzin
{

    // Forward declaration of resource ids

    enum class PsoId : uint32_t;
    enum class TextureId : uint32_t;

    struct RasterizerState
    {
        D3D12_FILL_MODE m_D3D12FillMode = D3D12_FILL_MODE_SOLID;
        D3D12_CULL_MODE m_D3D12CullMode = D3D12_CULL_MODE_BACK;
        bool m_IsIndexOrderClockwise = true;
    };

    struct DepthState
    {
        static_assert(D3D12_DEPTH_WRITE_MASK_ZERO == false);
        static_assert(D3D12_DEPTH_WRITE_MASK_ALL == true);

        bool m_IsEnabled = false;
        bool m_IsWriteEnabled = false;
        D3D12_COMPARISON_FUNC m_D3D12ComparisonFunction = D3D12_COMPARISON_FUNC_LESS;
    };

    struct BlendState
    {
        struct Equation
        {
            D3D12_BLEND m_D3D12SourceFactor = D3D12_BLEND_ONE;
            D3D12_BLEND m_D3D12DestinationFactor = D3D12_BLEND_ZERO;
            D3D12_BLEND_OP m_D3D12Operation = D3D12_BLEND_OP_ADD;
        };

        struct RenderTargetState
        {
            bool m_IsEnabled = false;
            Equation m_ColorEquation;
            Equation m_AlphaEquation;
        };

        std::vector<RenderTargetState> m_RenderTargetStates;
    };

    uint32_t GetDxgiFormatSizeInBytes(DXGI_FORMAT dxgiFormat);

}
