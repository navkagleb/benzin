#pragma once

namespace spieler::renderer
{

    enum class DepthTest : bool
    {
        Disabled = false,
        Enabled = true
    };

    enum class DepthWriteState : bool
    {
        Disabled = false,
        Enabled = true
    };

    enum class ComparisonFunction : uint8_t
    {
        Never = D3D12_COMPARISON_FUNC_NEVER,
        Less = D3D12_COMPARISON_FUNC_LESS,
        Equal = D3D12_COMPARISON_FUNC_EQUAL,
        LessEqual = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        Greate = D3D12_COMPARISON_FUNC_GREATER,
        NotEqual = D3D12_COMPARISON_FUNC_NOT_EQUAL,
        GreateEqual = D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        Always = D3D12_COMPARISON_FUNC_ALWAYS
    };

    struct DepthState
    {
        DepthTest DepthTest{ DepthTest::Enabled };
        DepthWriteState DepthWriteState{ DepthWriteState::Enabled };
        ComparisonFunction ComparisonFunction{ ComparisonFunction::Less };
    };

    enum class StencilTest : bool
    {
        Disabled = false,
        Enabled = true
    };

    enum class StencilOperation : uint8_t
    {
        Keep = D3D12_STENCIL_OP_KEEP,
        Zero = D3D12_STENCIL_OP_ZERO,
        Replace = D3D12_STENCIL_OP_REPLACE,
        IncreateSatureated = D3D12_STENCIL_OP_INCR_SAT,
        DescreaseSaturated = D3D12_STENCIL_OP_DECR_SAT,
        Invert = D3D12_STENCIL_OP_INVERT,
        Increase = D3D12_STENCIL_OP_INCR,
        Decrease = D3D12_STENCIL_OP_DECR
    };

    struct StencilBehaviour
    {
        StencilOperation StencilFailOperation{ StencilOperation::Keep };
        StencilOperation DepthFailOperation{ StencilOperation::Keep };
        StencilOperation PassOperation{ StencilOperation::Keep };
        ComparisonFunction StencilFunction{ ComparisonFunction::Always };
    };

    struct StencilState
    {
        StencilTest StencilTest{ StencilTest::Disabled };
        uint8_t ReadMask{ 0xff };
        uint8_t WriteMask{ 0xff };
        StencilBehaviour FrontFace;
        StencilBehaviour BackFace;
    };

    struct DepthStencilState
    {
        DepthState DepthState;
        StencilState StencilState;
    };

} // namespace spieler::renderer