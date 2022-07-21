#pragma once

namespace spieler::renderer
{

    struct BlendState
    {
        enum class Operation : uint8_t
        {
            Add,
            Substract,
            Min,
            Max
        };

        enum class ColorFactor : uint8_t
        {
            Zero,
            One,
            SourceColor,
            InverseSourceColor,
            SourceAlpha,
            InverseSourceAlpha,
            DesctinationAlpha,
            InverseDestinationAlpha,
            DestinationColor,
            InverseDestinationColor,
            SourceAlphaSaturated,
            BlendFactor,
            InverseBlendFactor
        };

        enum class AlphaFactor : uint8_t
        {
            Zero,
            One,
            SourceAlpha,
            InverseSourceAlpha,
            DesctinationAlpha,
            InverseDestinationAlpha,
            SourceAlphaSaturated,
            BlendFactor,
            InverseBlendFactor
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

} // namespace spieler::renderer
