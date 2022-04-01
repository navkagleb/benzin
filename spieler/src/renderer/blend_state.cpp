#include "renderer/blend_state.hpp"

namespace Spieler
{

    RenderTargetBlendProps RenderTargetBlendProps::CreateNoneBlending(BlendChannel channels)
    {
        RenderTargetBlendProps result;
        result.Channels = channels;

        return result;
    }

    RenderTargetBlendProps RenderTargetBlendProps::CreateDefaultBlending(const BlendColorEquation& colorEquation, const BlendAlphaEquation& alphaEquation, BlendChannel channels)
    {
        RenderTargetBlendProps result;
        result.ColorEquation = colorEquation;
        result.AlphaEquation = alphaEquation;
        result.Channels = channels;

        return result;
    }

    RenderTargetBlendProps RenderTargetBlendProps::CreateLogicBlending(const BlendLogicOperation& logicOperation, BlendChannel channels)
    {
        RenderTargetBlendProps result;
        result.LogicOperation = logicOperation;
        result.Channels = channels;

        return result;
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

} // namespace Spieler