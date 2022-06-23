#pragma once

#include <spieler/renderer/texture.hpp>
#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/shader.hpp>

namespace sandbox
{

    class SobelFilterPass
    {
    public:
        SobelFilterPass(uint32_t width, uint32_t height);

    public:
        const spieler::renderer::Texture2D& GetOutputTexture() const { return m_OutputTexture; }

    public:
        void Execute(spieler::renderer::Texture2D& input);

        void OnResize(uint32_t width, uint32_t height);

    private:
        void InitOutputTexture(uint32_t width, uint32_t height);
        void InitRootSignature();
        void InitPipelineState();

    private:
        spieler::renderer::Texture2D m_OutputTexture;
        spieler::renderer::RootSignature m_RootSignature;
        spieler::renderer::ComputePipelineState m_PipelineState;
        spieler::renderer::ShaderLibrary m_ShaderLibrary;
    };

} // namespace sandbox
