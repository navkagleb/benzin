#pragma once

#include <cstdint>
#include <vector>

#include <d3d12.h>

namespace Spieler
{

    enum class BlendOperation : std::uint8_t
    {
        Add = D3D12_BLEND_OP_ADD,
        Substract = D3D12_BLEND_OP_SUBTRACT,
        ReverseSubstact = D3D12_BLEND_OP_REV_SUBTRACT,
        Min = D3D12_BLEND_OP_MIN,
        Max = D3D12_BLEND_OP_MAX
    };

    enum class BlendLogicOperation : std::uint8_t
    {
        Clear = D3D12_LOGIC_OP_CLEAR,
        Set = D3D12_LOGIC_OP_SET,
        Copy = D3D12_LOGIC_OP_COPY,
        CopyInverted = D3D12_LOGIC_OP_COPY_INVERTED,
        None = D3D12_LOGIC_OP_NOOP,
        Invert = D3D12_LOGIC_OP_INVERT,
        And = D3D12_LOGIC_OP_AND,
        Nand = D3D12_LOGIC_OP_NAND,
        Or = D3D12_LOGIC_OP_OR,
        Nor = D3D12_LOGIC_OP_NOR,
        Xor = D3D12_LOGIC_OP_XOR,
        Equality = D3D12_LOGIC_OP_EQUIV,
        AndReverse = D3D12_LOGIC_OP_AND_REVERSE,
        AndInverted = D3D12_LOGIC_OP_AND_INVERTED,
        OrReverse = D3D12_LOGIC_OP_OR_REVERSE,
        OrInverted = D3D12_LOGIC_OP_OR_INVERTED
    };

    enum class BlendColorFactor : std::uint8_t
    {
        Zero = D3D12_BLEND_ZERO,
        One = D3D12_BLEND_ONE,
        SourceColor = D3D12_BLEND_SRC_COLOR,
        InverseSourceColor= D3D12_BLEND_INV_SRC_COLOR,
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

    enum class BlendAlphaFactor : std::uint8_t
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
    struct BlendEquation
    {
        Factor SourceFactor;
        Factor DestinationFactor;
        BlendOperation Operation;
    };

    using BlendColorEquation = BlendEquation<BlendColorFactor>;
    using BlendAlphaEquation = BlendEquation<BlendAlphaFactor>;

    enum BlendChannel : std::uint8_t
    {
        BlendChannel_None = 0,
        BlendChannel_Red = D3D12_COLOR_WRITE_ENABLE_RED,
        BlendChannel_Green = D3D12_COLOR_WRITE_ENABLE_GREEN,
        BlendChannel_Blue = D3D12_COLOR_WRITE_ENABLE_BLUE,
        BlendChannel_Alpha = D3D12_COLOR_WRITE_ENABLE_ALPHA,
        BlendChannel_All = D3D12_COLOR_WRITE_ENABLE_ALL
    };

    enum class RenderTargetBlendingType : std::uint8_t
    {
        None,
        Default,
        Logic
    };

    struct RenderTargetBlendProps
    {
        RenderTargetBlendingType Type{ RenderTargetBlendingType::None };
        BlendColorEquation ColorEquation;
        BlendAlphaEquation AlphaEquation;
        BlendLogicOperation LogicOperation{ BlendLogicOperation::None };
        BlendChannel Channels{ BlendChannel_None };

        static RenderTargetBlendProps CreateNoneBlending(BlendChannel channels = BlendChannel_All);
        static RenderTargetBlendProps CreateDefaultBlending(const BlendColorEquation& colorEquation, const BlendAlphaEquation& alphaEquation, BlendChannel channels = BlendChannel_All);
        static RenderTargetBlendProps CreateLogicBlending(const BlendLogicOperation& logicOperation, BlendChannel channels = BlendChannel_All);
    };

    struct BlendState
    {
        enum class AlphaToCoverageState AlphaToCoverageState{ AlphaToCoverageState::Disabled };
        enum class IndependentBlendState IndependentBlendState{ IndependentBlendState::Disabled };
        std::vector<RenderTargetBlendProps> RenderTargetProps;

        static BlendState CreateNoneBlending(BlendChannel channels = BlendChannel_All);
        static BlendState CreateDefaultBlending(enum class AlphaToCoverageState alphaToCoverageState, const BlendColorEquation& colorEquation, const BlendAlphaEquation& alphaEquation, BlendChannel channels = BlendChannel_All);
        static BlendState CreateLogicBlending(enum class AlphaToCoverageState alphaToCoverageState, const BlendLogicOperation& logicOperation, BlendChannel channels = BlendChannel_All);
    };

} // namespace Spieler