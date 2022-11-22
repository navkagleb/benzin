#pragma once

#include <spieler/system/window_event.hpp>

#include <spieler/graphics/texture.hpp>
#include <spieler/graphics/root_signature.hpp>
#include <spieler/graphics/pipeline_state.hpp>
#include <spieler/graphics/shader.hpp>
#include <spieler/graphics/graphics_command_list.hpp>
#include <spieler/graphics/device.hpp>

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
            spieler::RootSignature RootSignature;
            spieler::PipelineState PSO;
        };

    public:
        static int32_t GetBlurRadius(float sigma) { return static_cast<int32_t>(std::ceilf(sigma * 2.0f)); }

    public:
        BlurPass(spieler::Device& device, uint32_t width, uint32_t height);

    public:
        spieler::Texture& GetOutput() { return m_BlurMaps[0]; }

    public:
        bool Execute(spieler::GraphicsCommandList& graphicsCommandList, spieler::TextureResource& input, const BlurPassExecuteProps& props);

        void OnResize(spieler::Device& device, uint32_t width, uint32_t height);

    private:
        void InitTextures(spieler::Device& device, uint32_t width, uint32_t height);
        void InitHorizontalPass(spieler::Device& device);
        void InitVerticalPass(spieler::Device& device);

    private:
        BlurPassDirection m_HorizontalPass;
        BlurPassDirection m_VerticalPass;

        std::array<spieler::Texture, 2> m_BlurMaps;
        std::unordered_map<std::string, std::shared_ptr<spieler::Shader>> m_ShaderLibrary;
    };

} // namespace sandbox
