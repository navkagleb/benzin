#pragma once

#include "benzin/graphics/api/common.hpp"

namespace benzin
{

    struct BlendState
    {
        enum class Operation : std::underlying_type_t<D3D12_BLEND_OP>
        {
            Add = D3D12_BLEND_OP_ADD,
            Substract = D3D12_BLEND_OP_SUBTRACT,
            Min = D3D12_BLEND_OP_MIN,
            Max = D3D12_BLEND_OP_MAX,
        };

        enum class ColorFactor : std::underlying_type_t<D3D12_BLEND>
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
            InverseBlendFactor = D3D12_BLEND_INV_BLEND_FACTOR,
        };

        enum class AlphaFactor : std::underlying_type_t<D3D12_BLEND>
        {
            Zero = D3D12_BLEND_ZERO,
            One = D3D12_BLEND_ONE,
            SourceAlpha = D3D12_BLEND_SRC_ALPHA,
            InverseSourceAlpha = D3D12_BLEND_INV_SRC_ALPHA,
            DesctinationAlpha = D3D12_BLEND_DEST_ALPHA,
            InverseDestinationAlpha = D3D12_BLEND_INV_DEST_ALPHA,
            SourceAlphaSaturated = D3D12_BLEND_SRC_ALPHA_SAT,
            BlendFactor = D3D12_BLEND_BLEND_FACTOR,
            InverseBlendFactor = D3D12_BLEND_INV_BLEND_FACTOR,
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
            bool IsEnabled{ false };
            ColorEquation ColorEquation;
            AlphaEquation AlphaEquation;
            Channels Channels{ Channels::All };
        };

        AlphaToCoverageState AlphaToCoverageState{ AlphaToCoverageState::Disabled };
        IndependentBlendState IndependentBlendState{ IndependentBlendState::Disabled };
        std::vector<RenderTargetState> RenderTargetStates;

        static const BlendState& GetDefault();
    };

    struct DepthState
    {
        static_assert(D3D12_DEPTH_WRITE_MASK_ZERO == false);
        static_assert(D3D12_DEPTH_WRITE_MASK_ALL == true);

        bool IsEnabled{ true };
        bool IsWriteEnabled{ true };
        ComparisonFunction ComparisonFunction{ ComparisonFunction::Less };

        static const DepthState& GetDefault();
        static const DepthState& GetLess();
        static const DepthState& GetLessEqual();
        static const DepthState& GetGreaterEqual();
        static const DepthState& GetDisabled();
    };

    struct StencilState
    {
        enum class Operation : std::underlying_type_t<D3D12_STENCIL_OP>
        {
            Keep = D3D12_STENCIL_OP_KEEP,
            Zero = D3D12_STENCIL_OP_ZERO,
            Replace = D3D12_STENCIL_OP_REPLACE,
            IncreateSatureated = D3D12_STENCIL_OP_INCR_SAT,
            DescreaseSaturated = D3D12_STENCIL_OP_DECR_SAT,
            Invert = D3D12_STENCIL_OP_INVERT,
            Increase = D3D12_STENCIL_OP_INCR,
            Decrease = D3D12_STENCIL_OP_DECR,
        };

        struct Behaviour
        {
            Operation StencilFailOperation{ Operation::Keep };
            Operation DepthFailOperation{ Operation::Keep };
            Operation PassOperation{ Operation::Keep };
            ComparisonFunction StencilFunction{ ComparisonFunction::Always };
        };

        bool IsEnabled{ false };
        uint8_t ReadMask{ 0xff };
        uint8_t WriteMask{ 0xff };
        Behaviour FrontFaceBehaviour;
        Behaviour BackFaceBehaviour;

        static const StencilState& GetDefault();
    };

    struct RasterizerState
    {
        enum class FillMode : std::underlying_type_t<D3D12_FILL_MODE>
        {
            Wireframe = D3D12_FILL_MODE_WIREFRAME,
            Solid = D3D12_FILL_MODE_SOLID,
        };

        enum class CullMode : std::underlying_type_t<D3D12_CULL_MODE>
        {
            None = D3D12_CULL_MODE_NONE,
            Front = D3D12_CULL_MODE_FRONT,
            Back = D3D12_CULL_MODE_BACK,
        };

        enum class TriangleOrder : bool
        {
            Clockwise,
            CounterClockwise,
        };

        FillMode FillMode{ FillMode::Solid };
        CullMode CullMode{ CullMode::Back };
        TriangleOrder TriangleOrder{ TriangleOrder::Clockwise };
        int32_t DepthBias{ D3D12_DEFAULT_DEPTH_BIAS }; // In Shader = DepthBias / 2 ^ 24
        float DepthBiasClamp{ D3D12_DEFAULT_DEPTH_BIAS_CLAMP };
        float SlopeScaledDepthBias{ D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS };

        static const RasterizerState& GetDefault();
        static const RasterizerState& GetSolidNoCulling();
    };

} // namespace benzin
