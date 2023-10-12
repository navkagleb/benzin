#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/render_states.hpp"

namespace benzin
{

    namespace
    {

        constexpr DepthState g_LessEqualDepthState
        {
            .ComparisonFunction = ComparisonFunction::LessEqual,
        };

        constexpr DepthState g_GreaterEqualDepthState
        {
            .ComparisonFunction = ComparisonFunction::GreaterEqual,
        };

        constexpr DepthState g_DisabledDepthState
        {
            .IsEnabled = false,
            .IsWriteEnabled = false,
        };

        constexpr RasterizerState g_SolidNoCullingRasterizerState
        {
            .CullMode = CullMode::None,
        };

    } // anonymous namespace

    const DepthState& DepthState::GetLessEqual()
    {
        return g_LessEqualDepthState;
    }

    const DepthState& DepthState::GetGreaterEqual()
    {
        return g_GreaterEqualDepthState;
    }

    const DepthState& DepthState::GetDisabled()
    {
        return g_DisabledDepthState;
    }

    const RasterizerState& RasterizerState::GetSolidNoCulling()
    {
        return g_SolidNoCullingRasterizerState;
    }

} // namespace benzin
