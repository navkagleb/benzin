#pragma once

#include <benzin/graphics/device.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/texture.hpp>

namespace sandbox
{

    class IBLTextureGenerator
    {
    public:
        IBLTextureGenerator(benzin::Device& device);

    public:
        benzin::Texture* GenerateIrradianceTexture(const benzin::Texture& cubeTexture) const;

    private:
        benzin::Device& m_Device;

        std::unique_ptr<benzin::PipelineState> m_IrradiancePipelineState;
    };

} // namespace sandbox
