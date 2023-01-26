#pragma once

#include "spieler/graphics/common.hpp"

namespace spieler
{

    struct DepthState
    {
        enum class TestState : bool
        {
            Disabled = false,
            Enabled = true
        };

        enum class WriteState : bool
        {
            Disabled = D3D12_DEPTH_WRITE_MASK_ZERO,
            Enabled = D3D12_DEPTH_WRITE_MASK_ALL
        };

        TestState TestState{ TestState::Disabled };
        WriteState WriteState{ WriteState::Enabled };
        ComparisonFunction ComparisonFunction{ ComparisonFunction::Less };
    };

    struct StencilState
    {
        enum class TestState : bool
        {
            Disabled = false,
            Enabled = true
        };

        enum class Operation : uint8_t
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

        struct Behaviour
        {
            Operation StencilFailOperation{ Operation::Keep };
            Operation DepthFailOperation{ Operation::Keep };
            Operation PassOperation{ Operation::Keep };
            ComparisonFunction StencilFunction{ ComparisonFunction::Always };
        };

        TestState TestState{ TestState::Disabled };
        uint8_t ReadMask{ 0xff };
        uint8_t WriteMask{ 0xff };
        Behaviour FrontFaceBehaviour;
        Behaviour BackFaceBehaviour;
    };

    struct DepthStencilState
    {
        DepthState DepthState;
        StencilState StencilState;
    };

} // namespace spieler
