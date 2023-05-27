#include "bootstrap.hpp"

#include "ibl_texture_generator.hpp"

#include <benzin/graphics/api/command_queue.hpp>
#include <benzin/graphics/api/pipeline_state.hpp>
#include <benzin/graphics/api/texture.hpp>

namespace sandbox
{

	IBLTextureGenerator::IBLTextureGenerator(benzin::Device& device)
		: m_Device{ device }
	{
		const benzin::PipelineState::ComputeConfig config
		{
			.ComputeShader{ "ibl_texture.hlsl", "CS_Main" },
		};

		m_IrradiancePipelineState = std::make_unique<benzin::PipelineState>(m_Device, config);
		m_IrradiancePipelineState->SetDebugName("IrradianceGenerator");
	}

	benzin::TextureResource* IBLTextureGenerator::GenerateIrradianceTexture(const benzin::TextureResource& cubeTexture) const
	{
		const uint32_t textureSize = 64;
		const uint32_t textureArraySize = 6;

		const benzin::TextureResource::Config config
		{
			.Type{ benzin::TextureResource::Type::Texture2D },
			.Width{ textureSize },
			.Height{ textureSize },
			.ArraySize{ textureArraySize },
			.MipCount{ 1 },
			.IsCubeMap{ true },
			.Format{ benzin::GraphicsFormat::RGBA32Float },
			.Flags{ benzin::TextureResource::Flags::BindAsUnorderedAccess },
		};

		auto* irradianceTexture = new benzin::TextureResource{ m_Device, config };
		irradianceTexture->SetDebugName("Irradiance");
		irradianceTexture->PushUnorderedAccessView({ .StartElementIndex{ 0 }, .ElementCount{ textureArraySize } });

		{
			enum class RootConstant : uint32_t
			{
				InputCubeTextureIndex,
				OutIrradianceTextureIndex,
			};

			benzin::CommandQueueScope computeCommandQueue{ m_Device.GetComputeCommandQueue() };
			auto& computeCommandList = computeCommandQueue->GetCommandList();

			computeCommandList.SetPipelineState(*m_IrradiancePipelineState);
			computeCommandList.SetResourceBarrier(*irradianceTexture, benzin::Resource::State::UnorderedAccess);

			BENZIN_ASSERT(cubeTexture.HasShaderResourceView());
			computeCommandList.SetRootShaderResource(RootConstant::InputCubeTextureIndex, cubeTexture.GetShaderResourceView());
			computeCommandList.SetRootUnorderedAccess(RootConstant::OutIrradianceTextureIndex, irradianceTexture->GetUnorderedAccessView());

			computeCommandList.Dispatch({ textureSize, textureSize, 6 }, { 8, 8, 1 });

			computeCommandList.SetResourceBarrier(*irradianceTexture, benzin::Resource::State::Present);
		}

		return irradianceTexture;
	}

} // namespace sandbox
