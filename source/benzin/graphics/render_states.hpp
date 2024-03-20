#pragma once

#include "benzin/core/enum_flags.hpp"
#include "benzin/graphics/common.hpp"

namespace benzin
{

    // RasterizerState

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

    struct RasterizerState
    {
        FillMode FillMode = FillMode::Solid;
        CullMode CullMode = CullMode::Back;
        TriangleOrder TriangleOrder = TriangleOrder::Clockwise;
        int32_t DepthBias = D3D12_DEFAULT_DEPTH_BIAS; // In Shader = DepthBias / 2 ^ 24
        float DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        float SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    };

    // DepthState

    struct DepthState
    {
        static_assert(D3D12_DEPTH_WRITE_MASK_ZERO == false);
        static_assert(D3D12_DEPTH_WRITE_MASK_ALL == true);

        bool IsEnabled = true;
        bool IsWriteEnabled = true;
        ComparisonFunction ComparisonFunction = ComparisonFunction::Less;
    };

    // StencilState

    enum class StencilOperation : std::underlying_type_t<D3D12_STENCIL_OP>
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

    struct StencilBehaviour
    {
        StencilOperation StencilFailOperation = StencilOperation::Keep;
        StencilOperation DepthFailOperation = StencilOperation::Keep;
        StencilOperation PassOperation = StencilOperation::Keep;
        ComparisonFunction StencilFunction = ComparisonFunction::Always;
    };

    struct StencilState
    {
        bool IsEnabled = false;
        uint8_t ReadMask = std::numeric_limits<uint8_t>::max();
        uint8_t WriteMask = std::numeric_limits<uint8_t>::max();
        StencilBehaviour FrontFaceBehaviour;
        StencilBehaviour BackFaceBehaviour;
    };

    // BlendState

    enum class BlendOperation : std::underlying_type_t<D3D12_BLEND_OP>
    {
        Add = D3D12_BLEND_OP_ADD,
        Substract = D3D12_BLEND_OP_SUBTRACT,
        Min = D3D12_BLEND_OP_MIN,
        Max = D3D12_BLEND_OP_MAX,
    };

    enum class BlendColorFactor : std::underlying_type_t<D3D12_BLEND>
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

    enum class BlendAlphaFactor : std::underlying_type_t<D3D12_BLEND>
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

    template <typename BlendFactor>
    struct BlendEquation
    {
        BlendFactor SourceFactor = BlendFactor::One;
        BlendFactor DestinationFactor = BlendFactor::One;
        BlendOperation Operation = BlendOperation::Add;
    };

    using BlendColorEquation = BlendEquation<BlendColorFactor>;
    using BlendAlphaEquation = BlendEquation<BlendAlphaFactor>;

    enum class ColorChannelFlag : uint8_t
    {
        Red = ToBit(0),
        Green = ToBit(1),
        Blue = ToBit(2),
        Alpha = ToBit(3),
        All = Red | Green | Blue | Alpha,
    };
    BenzinDefineFlagsForBitEnum(ColorChannelFlag);

    struct BlendState
    {
        struct RenderTargetState
        {
            bool IsEnabled = false;
            BlendColorEquation ColorEquation;
            BlendAlphaEquation AlphaEquation;
            ColorChannelFlags ColorChannelFlags = ColorChannelFlag::All;
        };

        bool IsAlphaToCoverageStateEnabled = false;
        bool IsIndependentBlendStateEnabled = false;
        std::vector<RenderTargetState> RenderTargetStates;
    };

} // namespace benzin
