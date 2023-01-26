#pragma once

namespace spieler
{

    struct BlendState
    {
        enum class Operation : uint8_t
        {
            Add = D3D12_BLEND_OP_ADD,
            Substract = D3D12_BLEND_OP_SUBTRACT,
            Min = D3D12_BLEND_OP_MIN,
            Max = D3D12_BLEND_OP_MAX
        };

        enum class ColorFactor : uint8_t
        {
            Zero = D3D12_BLEND_ZERO,
            One = D3D12_BLEND_ONE,
            SourceColor = D3D12_BLEND_SRC_COLOR,
            InverseSourceColor = D3D12_BLEND_INV_SRC_COLOR,
            SourceAlpha = D3D12_BLEND_SRC_ALPHA,
            InverseSourceAlpha = D3D12_BLEND_INV_SRC_ALPHA,
            DesctinationAlpha = D3D12_BLEND_DEST_ALPHA,
            InverseDestinationAlpha = D3D12_BLEND_INV_DEST_ALPHA,
            DestinationColor = D3D12_BLEND_DEST_COLOR,
            InverseDestinationColor = D3D12_BLEND_INV_DEST_COLOR,
            SourceAlphaSaturated = D3D12_BLEND_SRC_ALPHA_SAT,
            BlendFactor = D3D12_BLEND_BLEND_FACTOR,
            InverseBlendFactor = D3D12_BLEND_INV_BLEND_FACTOR
        };

        enum class AlphaFactor : uint8_t
        {
            Zero = D3D12_BLEND_ZERO,
            One = D3D12_BLEND_ONE,
            SourceAlpha = D3D12_BLEND_SRC_ALPHA,
            InverseSourceAlpha = D3D12_BLEND_INV_SRC_ALPHA,
            DesctinationAlpha = D3D12_BLEND_DEST_ALPHA,
            InverseDestinationAlpha = D3D12_BLEND_INV_DEST_ALPHA,
            SourceAlphaSaturated = D3D12_BLEND_SRC_ALPHA_SAT,
            BlendFactor = D3D12_BLEND_BLEND_FACTOR,
            InverseBlendFactor = D3D12_BLEND_INV_BLEND_FACTOR
        };

        enum class AlphaToCoverageState : bool
        {
            Enabled = true,
            Disabled = false
        };

        enum class IndependentBlendState : bool
        {
            Enabled = true,
            Disabled = false
        };

        template <typename Factor>
        struct Equation
        {
            Factor SourceFactor{ Factor::One };
            Factor DestinationFactor{ Factor::One };
            Operation Operation{ Operation::Add };
        };

        using ColorEquation = Equation<ColorFactor>;
        using AlphaEquation = Equation<AlphaFactor>;

        enum class Channels : uint8_t
        {
            None = 0 << 0,
            Red = 1 << 0,
            Green = 1 << 1,
            Blue = 1 << 2,
            Alpha = 1 << 3,
            All = None | Red | Green | Blue | Alpha
        };

        struct RenderTargetState
        {
            enum class State : bool
            {
                Disabled = false,
                Enabled = true
            };

            State State{ State::Disabled };
            ColorEquation ColorEquation;
            AlphaEquation AlphaEquation;
            Channels Channels{ Channels::None };
        };

        AlphaToCoverageState AlphaToCoverageState{ AlphaToCoverageState::Disabled };
        IndependentBlendState IndependentBlendState{ IndependentBlendState::Disabled };
        std::vector<RenderTargetState> RenderTargetStates;
    };

} // namespace spieler
