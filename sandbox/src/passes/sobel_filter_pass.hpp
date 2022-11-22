#pragma once

#include <spieler/graphics/texture.hpp>
#include <spieler/graphics/root_signature.hpp>
#include <spieler/graphics/pipeline_state.hpp>
#include <spieler/graphics/shader.hpp>
#include <spieler/graphics/graphics_command_list.hpp>

namespace sandbox
{

    class SobelFilterPass
    {
    public:
        SobelFilterPass(spieler::Device& device, uint32_t width, uint32_t height);

    public:
        const spieler::Texture& GetOutputTexture() const { return m_OutputTexture; }

    public:
        void Execute(spieler::GraphicsCommandList& graphicsCommandList, spieler::Texture& input);

        void OnResize(spieler::Device& device, uint32_t width, uint32_t height);

    private:
        void InitOutputTexture(spieler::Device& device, uint32_t width, uint32_t height);
        void InitRootSignature(spieler::Device& device);
        void InitPipelineState(spieler::Device& device);

    private:
        spieler::Texture m_OutputTexture;
        spieler::RootSignature m_RootSignature;
        spieler::ComputePipelineState m_PipelineState;
        std::shared_ptr<spieler::Shader> m_Shader;
    };

} // namespace sandbox
