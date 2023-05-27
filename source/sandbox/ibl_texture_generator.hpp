#pragma once

#include <benzin/graphics/api/device.hpp>
#include <benzin/graphics/api/pipeline_state.hpp>
#include <benzin/graphics/api/texture.hpp>

namespace sandbox
{

	class IBLTextureGenerator
	{
	public:
		IBLTextureGenerator(benzin::Device& device);

	public:
		benzin::TextureResource* GenerateIrradianceTexture(const benzin::TextureResource& cubeTexture) const;

	private:
		benzin::Device& m_Device;

		std::unique_ptr<benzin::PipelineState> m_IrradiancePipelineState;
	};

} // namespace sandbox
