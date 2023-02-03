#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/sampler.hpp"

namespace benzin
{

	namespace
	{

		constexpr Sampler g_PointWrapSampler
		{
			.Minification{ Sampler::TextureFilterType::Point },
			.Magnification{ Sampler::TextureFilterType::Point },
			.MipLevel{ Sampler::TextureFilterType::Point },
			.AddressU{ Sampler::TextureAddressMode::Wrap },
			.AddressV{ Sampler::TextureAddressMode::Wrap },
			.AddressW{ Sampler::TextureAddressMode::Wrap }
		};

		constexpr Sampler g_PointClampSampler
		{
			.Minification{ Sampler::TextureFilterType::Point },
			.Magnification{ Sampler::TextureFilterType::Point },
			.MipLevel{ Sampler::TextureFilterType::Point },
			.AddressU{ Sampler::TextureAddressMode::Clamp },
			.AddressV{ Sampler::TextureAddressMode::Clamp },
			.AddressW{ Sampler::TextureAddressMode::Clamp }
		};

		constexpr Sampler g_LinearWrapSampler
		{
			.Minification{ Sampler::TextureFilterType::Linear },
			.Magnification{ Sampler::TextureFilterType::Linear },
			.MipLevel{ Sampler::TextureFilterType::Linear },
			.AddressU{ Sampler::TextureAddressMode::Wrap },
			.AddressV{ Sampler::TextureAddressMode::Wrap },
			.AddressW{ Sampler::TextureAddressMode::Wrap }
		};

		constexpr Sampler g_LinearClampSampler
		{
			.Minification{ Sampler::TextureFilterType::Linear },
			.Magnification{ Sampler::TextureFilterType::Linear },
			.MipLevel{ Sampler::TextureFilterType::Linear },
			.AddressU{ Sampler::TextureAddressMode::Clamp },
			.AddressV{ Sampler::TextureAddressMode::Clamp },
			.AddressW{ Sampler::TextureAddressMode::Clamp }
		};

		constexpr Sampler g_AnisotropicWrapSampler
		{
			.Minification{ Sampler::TextureFilterType::Anisotropic },
			.Magnification{ Sampler::TextureFilterType::Anisotropic },
			.MipLevel{ Sampler::TextureFilterType::Anisotropic },
			.AddressU{ Sampler::TextureAddressMode::Clamp },
			.AddressV{ Sampler::TextureAddressMode::Clamp },
			.AddressW{ Sampler::TextureAddressMode::Clamp }
		};

		constexpr Sampler g_AnisotropicClampSampler
		{
			.Minification{ Sampler::TextureFilterType::Anisotropic },
			.Magnification{ Sampler::TextureFilterType::Anisotropic },
			.MipLevel{ Sampler::TextureFilterType::Anisotropic },
			.AddressU{ Sampler::TextureAddressMode::Clamp },
			.AddressV{ Sampler::TextureAddressMode::Clamp },
			.AddressW{ Sampler::TextureAddressMode::Clamp }
		};

	} // anonymous namespace

	const Sampler& Sampler::GetPointWrap()
	{
		return g_PointWrapSampler;
	}

	const Sampler& Sampler::GetPointClamp()
	{
		return g_PointClampSampler;
	}

	const Sampler& Sampler::GetLinearWrap()
	{
		return g_LinearWrapSampler;
	}

	const Sampler& Sampler::GetLinearClamp()
	{
		return g_LinearClampSampler;
	}

	const Sampler& Sampler::GetAnisotropicWrap()
	{
		return g_AnisotropicWrapSampler;
	}

	const Sampler& Sampler::GetAnisotropicClamp()
	{
		return g_AnisotropicClampSampler;
	}

	StaticSampler StaticSampler::GetPointWrap(const Shader::Register& shaderRegister)
	{
		return StaticSampler
		{
			.Sampler{ g_PointWrapSampler },
			.ShaderRegister{ shaderRegister }
		};
	}

	StaticSampler StaticSampler::GetPointClamp(const Shader::Register& shaderRegister)
	{
		return StaticSampler
		{
			.Sampler{ g_PointClampSampler },
			.ShaderRegister{ shaderRegister }
		};
	}

	StaticSampler StaticSampler::GetLinearWrap(const Shader::Register& shaderRegister)
	{
		return StaticSampler
		{
			.Sampler{ g_LinearWrapSampler },
			.ShaderRegister{ shaderRegister }
		};
	}

	StaticSampler StaticSampler::GetLinearClamp(const Shader::Register& shaderRegister)
	{
		return StaticSampler
		{
			.Sampler{ g_LinearClampSampler },
			.ShaderRegister{ shaderRegister }
		};
	}

	StaticSampler StaticSampler::GetAnisotropicWrap(const Shader::Register& shaderRegister)
	{
		return StaticSampler
		{
			.Sampler{ g_AnisotropicWrapSampler },
			.ShaderRegister{ shaderRegister }
		};
	}

	StaticSampler StaticSampler::GetAnisotropicClamp(const Shader::Register& shaderRegister)
	{
		return StaticSampler
		{
			.Sampler{ g_AnisotropicClampSampler },
			.ShaderRegister{ shaderRegister }
		};
	}

	StaticSampler StaticSampler::GetDefaultForShadow(const Shader::Register& shaderRegister)
	{
		return StaticSampler
		{
			.Sampler
			{
				.Minification{ Sampler::TextureFilterType::Linear },
				.Magnification{ Sampler::TextureFilterType::Linear },
				.MipLevel{ Sampler::TextureFilterType::Linear },
				.AddressU{ Sampler::TextureAddressMode::Border },
				.AddressV{ Sampler::TextureAddressMode::Border },
				.AddressW{ Sampler::TextureAddressMode::Border }
			},
			.ShaderRegister{ shaderRegister },
			.BorderColor{ BorderColor::OpaqueBlack },
			.MipLODBias{ 0.0f },
			.MaxAnisotropy{ 16 },
			.ComparisonFunction{ benzin::ComparisonFunction::LessEqual }
		};
	}

} // namespace benzin
