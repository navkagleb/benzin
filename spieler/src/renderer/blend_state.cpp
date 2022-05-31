#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/blend_state.hpp"

namespace spieler::renderer
{

    RenderTargetBlendProps RenderTargetBlendProps::CreateNoneBlending(BlendChannel channels)
    {
        return RenderTargetBlendProps
        {
            .Channels = channels
        };
    }

    RenderTargetBlendProps RenderTargetBlendProps::CreateDefaultBlending(const BlendColorEquation& colorEquation, const BlendAlphaEquation& alphaEquation, BlendChannel channels)
    {
        return RenderTargetBlendProps
        {
            .ColorEquation = colorEquation,
            .AlphaEquation = alphaEquation,
            .Channels = channels
        };
    }

    RenderTargetBlendProps RenderTargetBlendProps::CreateLogicBlending(const BlendLogicOperation& logicOperation, BlendChannel channels)
    {
        return RenderTargetBlendProps
        {
            .LogicOperation = logicOperation,
            .Channels = channels,
        };
    }

    BlendState BlendState::CreateNoneBlending(BlendChannel channels)
    {
        BlendState blendState;
        blendState.RenderTargetProps.push_back(RenderTargetBlendProps::CreateNoneBlending(channels));

        return blendState;
    }

    BlendState BlendState::CreateDefaultBlending(enum class AlphaToCoverageState alphaToCoverageState, const BlendColorEquation& colorEquation, const BlendAlphaEquation& alphaEquation, BlendChannel channels)
    {
        BlendState blendState;
        blendState.AlphaToCoverageState = alphaToCoverageState;
        blendState.RenderTargetProps.push_back(RenderTargetBlendProps::CreateDefaultBlending(colorEquation, alphaEquation, channels));

        return blendState;
    }

    BlendState BlendState::CreateLogicBlending(enum class AlphaToCoverageState alphaToCoverageState, const BlendLogicOperation& logicOperation, BlendChannel channels)
    {
        BlendState blendState;
        blendState.AlphaToCoverageState = alphaToCoverageState;
        blendState.RenderTargetProps.push_back(RenderTargetBlendProps::CreateLogicBlending(logicOperation, channels));

        return blendState;
    }

} // namespace spieler::renderer