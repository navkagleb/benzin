#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/render_states.hpp"

namespace benzin
{

	namespace
	{

		const BlendState g_DefaultBlendState
		{
			.RenderTargetStates{ BlendState::RenderTargetState{} }
		};

		constexpr DepthState g_LessDepthState;

		constexpr DepthState g_LessEqualDepthState
		{
			.ComparisonFunction{ ComparisonFunction::LessEqual }
		};

		constexpr DepthState g_DisabledDepthState
		{
			.TestState{ DepthState::TestState::Disabled },
			.WriteState{ DepthState::WriteState::Disabled }
		};

		constexpr StencilState g_DefaultStencilState;

		constexpr RasterizerState g_DefaultRasterizerState;

		constexpr RasterizerState g_SolidNoCullingRasterizerState
		{
			.CullMode{ RasterizerState::CullMode::None }
		};

	} // anonymous namespace

	const BlendState& BlendState::GetDefault()
	{
		return g_DefaultBlendState;
	}

	const DepthState& DepthState::GetDefault()
	{
		return g_LessDepthState;
	}

	const DepthState& DepthState::GetLess()
	{
		return g_LessDepthState;
	}

	const DepthState& DepthState::GetLessEqual()
	{
		return g_LessEqualDepthState;
	}

	const DepthState& DepthState::GetDisabled()
	{
		return g_DisabledDepthState;
	}

	const StencilState& StencilState::GetDefault()
	{
		return g_DefaultStencilState;
	}

	const RasterizerState& RasterizerState::GetDefault()
	{
		return g_DefaultRasterizerState;
	}

	const RasterizerState& RasterizerState::GetSolidNoCulling()
	{
		return g_SolidNoCullingRasterizerState;
	}

} // namespace benzin
