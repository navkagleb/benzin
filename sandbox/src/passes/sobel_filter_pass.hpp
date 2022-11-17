#pragma once

#include <spieler/graphics/texture.hpp>
#include <spieler/graphics/root_signature.hpp>
#include <spieler/graphics/pipeline_state.hpp>
#include <spieler/graphics/shader.hpp>

namespace sandbox
{

    class SobelFilterPass
    {
    public:
        SobelFilterPass(uint32_t width, uint32_t height);

    public:
        const spieler::Texture& GetOutputTexture() const { return m_OutputTexture; }

    public:
        void Execute(spieler::Texture& input);

        void OnResize(uint32_t width, uint32_t height);

    private:
        void InitOutputTexture(uint32_t width, uint32_t height);
        void InitRootSignature();
        void InitPipelineState();

    private:
        spieler::Texture m_OutputTexture;
        spieler::RootSignature m_RootSignature;
        spieler::ComputePipelineState m_PipelineState;
        std::shared_ptr<spieler::Shader> m_Shader;
    };

} // namespace sandbox
