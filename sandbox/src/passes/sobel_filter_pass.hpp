#pragma once

#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/root_signature.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/shader.hpp>
#include <benzin/graphics/graphics_command_list.hpp>

namespace sandbox
{

    class SobelFilterPass
    {
    public:
        SobelFilterPass(benzin::Device& device, uint32_t width, uint32_t height);

    public:
        const std::shared_ptr<benzin::TextureResource>& GetOutputTexture() const { return m_OutputTexture; }

    public:
        void Execute(benzin::GraphicsCommandList& graphicsCommandList, benzin::TextureResource& input);

        void OnResize(benzin::Device& device, uint32_t width, uint32_t height);

    private:
        void InitOutputTexture(benzin::Device& device, uint32_t width, uint32_t height);
        void InitRootSignature(benzin::Device& device);
        void InitPipelineState(benzin::Device& device);

    private:
        std::shared_ptr<benzin::TextureResource> m_OutputTexture;
        std::unique_ptr<benzin::RootSignature> m_RootSignature;
        std::unique_ptr<benzin::ComputePipelineState> m_PipelineState;
        std::unique_ptr<benzin::Shader> m_Shader;
    };

} // namespace sandbox
