#pragma once

#include <spieler/system/window_event.hpp>

#include <spieler/renderer/texture.hpp>
#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/shader.hpp>

namespace sandbox
{

    class BlurPass
    {
    public:
        BlurPass(uint32_t width, uint32_t height);

    public:
        spieler::renderer::Texture2D& GetOutput() { return m_BlurMaps[0]; }

    public:
        bool Execute(spieler::renderer::Texture2DResource& input, uint32_t width, uint32_t height, uint32_t blurCount);

        void OnResize(uint32_t width, uint32_t height);

    private:
        void InitTextures(uint32_t width, uint32_t height);
        void InitRootSignature();
        void InitPipelineStates();

    private:
        std::array<spieler::renderer::Texture2D, 2> m_BlurMaps;
        spieler::renderer::RootSignature m_RootSignature;
        spieler::renderer::ComputePipelineState m_HorizontalBlurPSO;
        spieler::renderer::ComputePipelineState m_VerticalBlurPSO;
        spieler::renderer::ShaderLibrary m_ShaderLibrary;
    };

} // namespace sandbox