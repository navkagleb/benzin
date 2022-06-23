#pragma once

#include <spieler/system/window_event.hpp>

#include <spieler/renderer/texture.hpp>
#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/shader.hpp>

namespace sandbox
{

    struct BlurPassExecuteProps
    {
        float HorizontalBlurSigma{ 0.0f };
        float VerticalBlurSigma{ 0.0f };
        uint32_t BlurCount{ 0 };
    };

    class BlurPass
    {
    private:
        struct BlurPassDirection
        {
            spieler::renderer::RootSignature RootSignature;
            spieler::renderer::PipelineState PSO;
        };

    public:
        static int32_t GetBlurRadius(float sigma) { return static_cast<int32_t>(std::ceilf(sigma * 2.0f)); }

    public:
        BlurPass(uint32_t width, uint32_t height);

    public:
        spieler::renderer::Texture2D& GetOutput() { return m_BlurMaps[0]; }

    public:
        bool Execute(spieler::renderer::Texture2DResource& input, const BlurPassExecuteProps& props);

        void OnResize(uint32_t width, uint32_t height);

    private:
        void InitTextures(uint32_t width, uint32_t height);
        void InitHorizontalPass();
        void InitVerticalPass();

    private:
        BlurPassDirection m_HorizontalPass;
        BlurPassDirection m_VerticalPass;

        std::array<spieler::renderer::Texture2D, 2> m_BlurMaps;
        spieler::renderer::ShaderLibrary m_ShaderLibrary;
    };

} // namespace sandbox